"""Read-only LM reference inventory. Names are candidates, not semantic parity."""
import argparse
import hashlib
import json
import re
import sys
import zipfile
from collections import Counter, defaultdict
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'third_party/custom_dsp_language/compile/src'))
from lh_compiler.function_blocks.registry import FunctionBlockRegistry


def inventory(archive):
    registry = FunctionBlockRegistry()
    registry.load_defaults()
    uses = defaultdict(list)
    with zipfile.ZipFile(archive) as package:
        extensions = Counter()
        for entry in package.infolist():
            if entry.is_dir():
                continue
            extensions[Path(entry.filename).suffix] += 1
            if not entry.filename.endswith('.inva'):
                continue
            source = package.read(entry).decode('gb18030')
            # Preserve line numbers while masking comments and quoted labels.
            source = re.sub(r'/\*.*?\*/|//[^\r\n]*|"[^"\r\n]*"',
                            lambda m: re.sub(r'[^\r\n]', ' ', m[0]), source, flags=re.S)
            for line, text in enumerate(source.splitlines(), 1):
                match = re.match(r'\s*(_[A-Za-z][A-Za-z0-9_]*)\b', text)
                if match:
                    uses[match[1]].append({'file': entry.filename, 'line': line})
    blocks = []
    for name, locations in sorted(uses.items()):
        candidate = registry.get(name.lstrip('_'))
        blocks.append({'lm_name': name, 'locations': locations,
                       'name_candidate': candidate.name if candidate else None,
                       'candidate_status': candidate.status if candidate else None,
                       'parameter_count': len(candidate.parameters) if candidate else None,
                       'semantic_parity': 'unverified'})
    return {'archive_sha256': hashlib.sha256(Path(archive).read_bytes()).hexdigest(),
            'extensions': dict(extensions), 'lm_block_count': len(blocks),
            'lh_block_count': len(registry),
            'lh_empty_parameter_blocks': [b.name for b in registry.list_all() if not b.parameters],
            'blocks': blocks}


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('archive', type=Path)
    parser.add_argument('output', type=Path)
    args = parser.parse_args()
    result = inventory(args.archive)
    args.output.write_text(json.dumps(result, ensure_ascii=False, indent=2) + '\n', encoding='utf-8')
    print(f"LM blocks: {result['lm_block_count']}; LH blocks: {result['lh_block_count']}")
