"""Extract local manual evidence; does not import or execute the LH compiler."""
from pathlib import Path
import hashlib
import json
import re
from pypdf import PdfReader

OUTPUT = Path(__file__).resolve().parent
plan_root = Path("C:/Users/Ryan/.codex/visualizations/2026/09/30/01a0eff5-759e-7ec1-b9d1-4f644bae8f8f")
feasibility = json.loads((plan_root / "LH-compiler-feasibility-2026-09-30.json").read_text(encoding="utf-8-sig"))
manuals = {
    "用户手册": Path("D:/Table/LM/LM2007用户手册(1).pdf"),
    "指南": Path("D:/Table/LM/高质量LM编程指南V1.05.pdf"),
}
readers = {name: PdfReader(path) for name, path in manuals.items()}
hashes = {name: hashlib.sha256(path.read_bytes()).hexdigest() for name, path in manuals.items()}
text_output = OUTPUT / "manual-evidence"
text_output.mkdir(exist_ok=True)
for block in feasibility["blocks"]:
    refs = []
    for name, raw_pages in re.findall(r"(用户手册|指南) PDF 第\s*([\d、–\- ]+)\s*页", block["evidence"]):
        pages = set()
        for part in raw_pages.split("、"):
            numbers = [int(value) for value in re.findall(r"\d+", part)]
            if len(numbers) == 2:
                pages.update(range(numbers[0], numbers[1] + 1))
            elif numbers:
                pages.add(numbers[0])
        for page in sorted(pages):
            reader = readers[name]
            if not 1 <= page <= len(reader.pages):
                raise ValueError(f"Manual page outside document: {name} {page}")
            filename = f"{'manual' if name == '用户手册' else 'guide'}-page-{page}.txt"
            (text_output / filename).write_text(reader.pages[page - 1].extract_text() or "", encoding="utf-8")
            refs.append({"path": str(manuals[name]), "sha256": hashes[name], "physicalPage": page,
                         "extractedText": f"manual-evidence/{filename}", "typeDirectionAndLayoutCertifiedForLH": False})
    evidence_path = OUTPUT / "function-contract-evidence" / f"{block['id']}-{block['name']}.json"
    record = json.loads(evidence_path.read_text(encoding="utf-8-sig"))
    record["manualPages"] = refs
    record["reviewCandidateContract"] = block["knownContract"]
    record["reviewAssessment"] = block["assessment"]
    record["manualExtractionScope"] = "Physical PDF pages, copied as evidence; legacy numeric tables do not certify LH operand encodings"
    evidence_path.write_text(json.dumps(record, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
print(f"Collected manual pages for {len(feasibility['blocks'])} block evidence records.")
