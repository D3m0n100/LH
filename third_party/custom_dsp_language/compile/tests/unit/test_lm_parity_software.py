"""Independent golden probes for software parity; no controller ABI assumptions."""
import pytest
from lh_compiler.compiler import LHCompiler
from lh_compiler.function_blocks.registry import FunctionBlockRegistry, FunctionBlockMeta, ParameterDef

@pytest.mark.parametrize('type_,expression,expected', [
    ('INT','2 + 3 * 4',14), ('INT','(2 + 3) * 4',20),
    ('INT','-2 + 3',1), ('INT','+2 + 3',5), ('INT','(-7) / 3',-2),
    ('INT','(-7) MOD 3',-1), ('INT','7 MOD (-3)',1),
    ('INT','6 AND 3',2), ('INT','4 OR 2',6), ('INT','7 XOR 3',4),
    ('INT','NOT 16#00F0',-241), ('WORD','NOT 16#00F0',65295),
    ('BYTE','NOT 16#F0',15), ('BOOL','TRUE AND NOT FALSE',1),
    ('BOOL','2 + 3 * 4 = 14',1), ('BOOL','3 <> 4',1),
    ('BOOL','TRUE OR FALSE AND FALSE',1), ('INT','1 OR 2 AND 4',1),
])
def test_constant_goldens(type_, expression, expected):
    result=LHCompiler().compile_string(f'PROGRAM P VAR x : {type_}; END_VAR x := {expression}; END_PROGRAM')
    assert result.success, result.errors
    assert result.instructions[-1].params == [expected]
    assert result.compile_only

@pytest.mark.parametrize('expression', ['TRUE + 2', 'TRUE AND 2', '1.0 AND 2.0', 'NOT 1.0',
                                         '1 / 0', '1 MOD 0', '32767 + 1', '1.5 MOD 1', '4294967296 * 0'])
def test_invalid_constants_rejected(expression):
    result=LHCompiler().compile_string(f'PROGRAM P VAR x : INT; END_VAR x := {expression}; END_PROGRAM')
    assert not result.success and result.errors

def test_registry_distinguishes_empty_contracts_and_noarg_operation():
    r=FunctionBlockRegistry();r.load_defaults()
    assert r.get('Nop').is_supported
    for name in ['Add','IntBuild','CommCANInit']:
        assert r.get(name).is_incomplete
        result=LHCompiler().compile_string(f'PROGRAM P VAR f : {name}; END_VAR f(); END_PROGRAM')
        assert not result.success
    issues=r.get('DrvT2PeriodIn').capability_state()
    assert not issues['definitionValid'] and not issues['targetExecutionVerified']
    assert any('65535' in e for e in issues['definitionErrors'])
    assert not FunctionBlockMeta('Test',1,1,[ParameterDef('P','INT',default_value=99999)]).capability_state()['definitionValid']

def test_precedence_diagnostic_only_for_unparenthesized_mixed_expression(tmp_path):
    compiler=LHCompiler()
    result=compiler.compile_string('PROGRAM P VAR x : INT; END_VAR x := 2 + 3 * 4; END_PROGRAM', str(tmp_path/'p.code'))
    assert result.success and result.warnings
    assert 'warning\t' in (tmp_path/'p.rep').read_text(encoding='utf-8')
    assert not compiler.compile_string('PROGRAM P VAR x : INT; END_VAR x := 2 + (3 * 4); END_PROGRAM').warnings

def test_cli_capability_json_matches_catalog():
    import json, subprocess, sys
    from pathlib import Path
    root=Path(__file__).resolve().parents[2]
    left=subprocess.run([sys.executable,str(root/'lmc.py'),'--capabilities-json'],capture_output=True,encoding='utf-8')
    right=subprocess.run([sys.executable,'-m','lh_compiler.cli.commands','list-blocks','--json'],capture_output=True,encoding='utf-8')
    assert left.returncode == right.returncode == 0, (left.stderr,right.stderr)
    assert json.loads(left.stdout)==json.loads(right.stdout)
    snippets=json.loads((root.parents[2]/'resources/snippets/default_snippets.json').read_text(encoding='utf-8'))
    states={b['name']:b for b in json.loads(left.stdout)}
    for snippet in snippets:
        assert snippet['metadata']['capabilityState']==states[snippet['metadata']['compilerName']]

def test_both_cli_entries_compile_new_precedence_with_stale_installed_grammar(tmp_path):
    import os, subprocess, sys
    from pathlib import Path
    root=Path(__file__).resolve().parents[2]
    source=tmp_path/'parity.lh'
    source.write_text('PROGRAM P VAR x : INT; END_VAR x := 2 + 3 * 4; END_PROGRAM',encoding='utf-8')
    for i,entry in enumerate([[sys.executable,str(root/'lmc.py')],
                              [sys.executable,'-m','lh_compiler.cli.commands','compile']]):
        output=tmp_path/f'cli{i}.code'
        environment=dict(os.environ, PYTHONPATH=str(root/'src'))
        run=subprocess.run([*entry,str(source),'-o',str(output)],cwd=tmp_path,env=environment,capture_output=True,encoding='utf-8')
        assert run.returncode==0,run.stdout+run.stderr
        records=[line.split() for line in output.read_text(encoding='utf-8').splitlines() if line.startswith('121 ')]
        assert len(records)==1 and records[0][-1]=='14'
        assert '优先级' in run.stdout

@pytest.mark.parametrize('expression', ['TRUE = 1', 'FALSE < 1', '0.0'])
def test_bool_comparison_and_assignment_do_not_coerce_real_or_integer_types(expression):
    result=LHCompiler().compile_string(f'PROGRAM P VAR x : BOOL; END_VAR x := {expression}; END_PROGRAM')
    assert not result.success and result.errors
