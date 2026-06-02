import sys
import json
import re
from pathlib import Path

ROOT = Path(__file__).resolve().parent
COMPILER_SRC = ROOT / 'third_party' / 'custom_dsp_language' / 'compile' / 'src'
if str(COMPILER_SRC) not in sys.path:
    sys.path.insert(0, str(COMPILER_SRC))

from lh_compiler.function_blocks.definitions import get_all_definitions


def camel_to_snake(name: str) -> str:
    name = name.lstrip('_')
    s1 = re.sub(r'(.)([A-Z][a-z]+)', r'\1_\2', name)
    s2 = re.sub(r'([a-z0-9])([A-Z])', r'\1_\2', s1)
    return re.sub(r'_+', '_', s2).lower()


def format_value(value):
    if isinstance(value, bool):
        return 'true' if value else 'false'
    if value is None:
        return '0'
    if isinstance(value, str):
        return json.dumps(value, ensure_ascii=False)
    return str(value)


def build_parameter_metadata(parameters):
    return [
        {
            'name': param.name,
            'dataType': param.data_type,
            'direction': param.direction,
            'offset': param.offset,
            'defaultValue': param.default_value,
            'description': param.description,
        }
        for param in parameters
    ]


def build_snippets():
    snippets = []
    for block in get_all_definitions():
        base_name = camel_to_snake(block.name) or 'block'
        variable_name = f'{base_name}_1'
        if block.parameters:
            args = ',\n    '.join(
                f'{param.name} = {format_value(param.default_value)}'
                for param in block.parameters
            )
            template_code = f'{variable_name} = {block.name}(\n    {args}\n);'
        else:
            template_code = f'{variable_name} = {block.name}();'

        snippets.append({
            'id': block.name,
            'name': block.name,
            'category': block.category or 'general',
            'description': block.description or block.name,
            'templateCode': template_code,
            'unit': '',
            'defaultPeriodMs': 20,
            'metadata': {
                'typeId': block.type_id,
                'memorySize': block.memory_size,
                'parameterCount': len(block.parameters),
                'parameters': build_parameter_metadata(block.parameters),
                'compilerName': block.name,
            },
        })
    return snippets


def main():
    output_path = ROOT / 'resources' / 'snippets' / 'default_snippets.json'
    output_path.write_text(
        json.dumps(build_snippets(), ensure_ascii=False, indent=2) + '\n',
        encoding='utf-8'
    )
    print(f'Wrote {output_path}')


if __name__ == '__main__':
    main()
