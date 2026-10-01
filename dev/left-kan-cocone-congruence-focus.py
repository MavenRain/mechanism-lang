"""Extract unchanged cocone law definitions for focused validation."""

import argparse
from pathlib import Path
import hashlib
import json
import re
import tempfile

parser = argparse.ArgumentParser(description=__doc__)
selection = parser.add_mutually_exclusive_group()
selection.add_argument('--cocone-equality', action='store_true')
selection.add_argument('--cocone-action', action='store_true')
selection.add_argument('--roundtrip', action='store_true')
selection.add_argument('--mediator-equality', action='store_true')
parser.add_argument('--contracts', action='store_true')
args = parser.parse_args()

root = Path(__file__).resolve().parents[1]
harness = (root / 'bend2/tests/prelude_left_kan_laws.bend').read_text()
templates = re.search(r'R\.read_all\(root,\s*\[([^\]]*)\]\)', harness)
if templates is None:
    raise ValueError('missing left Kan template inventory')
paths = re.findall(r'"([^"]+\.mech)"', templates.group(1))
if args.cocone_action:
    paths.append('test/fixtures/prelude/left-kan-cocone-action.mech')
if args.roundtrip or args.mediator_equality:
    paths = [path for path in paths if path not in {
        'test/fixtures/prelude/shared-left-kan-runtime.mech',
        'test/fixtures/prelude/left-kan-laws-runtime.mech'}]
    paths.append('test/fixtures/prelude/left-kan-roundtrip.mech')
if args.mediator_equality:
    paths.append('test/fixtures/prelude/left-kan-mediator-equality.mech')
groups = {}
global_blocks = []
aliases = {}
definitions = {}

def alias_rows(text):
    return dict((alias, group) for group, alias in re.findall(
        r'specialize\s+(\w+)\s+\([^\n]*?\)\s+as\s+(\w+)', text))

for relative in paths:
    text = (root / relative).read_text()
    group = re.search(r'^poly\s+\([^\n]*?\)\s+(?:group|mu)\s+(\w+)', text, re.M)
    if group:
        scope = group.group(1)
        matches = list(re.finditer(r'^def\s+(\w+)\s*:', text, re.M))
        header = text[:matches[0].start()] if matches else text
        end = text.rfind('\nend') if ' group ' in group.group(0) else len(text)
        rows = [(match.group(1), text[match.start():matches[i+1].start() if i+1<len(matches) else end])
                for i,match in enumerate(matches)]
        groups[scope] = (header, rows, text[end:] if end<len(text) else '', relative)
        aliases[scope] = alias_rows(text)
        definitions.update(((scope,name),body) for name,body in rows)
    else:
        matches = list(re.finditer(r'^(def|data|mu|specialize)\s+(\w+)', text, re.M))
        if not matches:
            global_blocks.append((None,text))
            continue
        global_blocks.append((None,text[:matches[0].start()]))
        for i, match in enumerate(matches):
            body = text[match.start():matches[i+1].start() if i+1<len(matches) else len(text)]
            if match.group(1)=='def':
                name=match.group(2)
                definitions[('',name)] = body
                global_blocks.append((name,body))
            else:
                global_blocks.append((None,body))
        aliases.setdefault('',{}).update(alias_rows(text))

def resolve(scope, word):
    if (scope,word) in definitions:
        return (scope,word)
    for alias, target in aliases.get(scope,{}).items():
        prefix=alias+'_'
        if word.startswith(prefix):
            return resolve(target,word[len(prefix):])
    return ('',word) if ('',word) in definitions else None

prefixes = ('lanMediatorEq',) if args.mediator_equality else (('lanRoundtrip',) if args.roundtrip else (('lanAction',) if args.cocone_action else (('lanCoconeEq',) if args.cocone_equality else ('lanPostCongr', 'lanPostReverseCongr'))))
seeds = [key for key in definitions if key[0]=='' and key[1].startswith(prefixes)]
if args.roundtrip and not args.contracts:
    runtime_exports = {'lanRoundtripCocone', 'lanRoundtripOtherCocone',
                       'lanRoundtripDesc', 'lanRoundtripOtherDesc'}
    seeds = [key for key in seeds if key[1] in runtime_exports]
if args.mediator_equality and not args.contracts:
    runtime_exports = {'lanMediatorEqReflect', 'lanMediatorEqOtherReflect',
                       'lanMediatorEqChoice', 'lanMediatorEqOtherChoice'}
    seeds = [key for key in seeds if key[1] in runtime_exports]
if args.contracts:
    seeds.extend(key for key in definitions if key[0]=='' and
                  key[1].startswith(('WideDescReflectsEq', 'WideDescChoiceEq') if args.mediator_equality else (('WidePostCoconeDesc', 'WideDescPostCocone') if args.roundtrip else ('WidePostCoconeEq' if args.cocone_action else ('WideCoconeEq' if args.cocone_equality else 'WidePostCoconeCongr')))))
    if args.cocone_action:
        seeds.append(('', 'OtherCocone'))
if not seeds:
    raise ValueError('no focused cocone law definitions found')
required=set(seeds)
pending=list(seeds)
while pending:
    key=pending.pop()
    for word in re.findall(r'\b[A-Za-z][A-Za-z0-9_]*\b',definitions[key]):
        candidate=resolve(key[0],word)
        if candidate and candidate not in required:
            required.add(candidate)
            pending.append(candidate)
# Keep the category equality family and its complete foundational members.
required.update(key for key in definitions if key[0]=='MechCategoryCore')
if args.roundtrip and not args.contracts:
    runtime_proofs = {('', name) for name in (
        'roundtripCoconeLaw', 'roundtripOtherCoconeLaw',
        'roundtripDescLaw', 'roundtripOtherDescLaw')}
    missing_proofs = runtime_proofs - required
    if missing_proofs:
        raise ValueError('runtime exports omit round-trip proofs: ' +
                         ', '.join(sorted(name for _, name in missing_proofs)))
if args.mediator_equality and not args.contracts:
    runtime_proofs = {('', name) for name in (
        'mediatorReflectLaw', 'mediatorOtherReflectLaw',
        'mediatorChoiceLaw', 'mediatorOtherChoiceLaw')}
    missing_proofs = runtime_proofs - required
    if missing_proofs:
        raise ValueError('runtime exports omit mediator equality proofs: ' +
                         ', '.join(sorted(name for _, name in missing_proofs)))
pieces=[]
for scope,(header,rows,tail,relative) in groups.items():
    pieces.append(header+''.join(body for name,body in rows if (scope,name) in required)+tail)
pieces.extend(body for name,body in global_blocks
              if (name is None or ('',name) in required)
              and (args.contracts or not re.match(
                  r'specialize\s+MechLeftKanLaws\s+[^\n]*\bas\s+Wide\b', body)))
text='\n'.join(pieces)
directory=root/('_bend2/left-kan-mediator-equality' if args.mediator_equality else ('_bend2/left-kan-roundtrip' if args.roundtrip else ('_bend2/left-kan-cocone-action' if args.cocone_action else ('_bend2/left-kan-cocone-equality' if args.cocone_equality else '_bend2/left-kan-cocone-congruence'))))
if args.contracts:
    directory = directory.with_name(directory.name + '-contracts')
directory.mkdir(parents=True, exist_ok=True)
with tempfile.NamedTemporaryFile(mode='w', dir=directory, delete=False) as output:
    output.write(text)
Path(output.name).replace(directory/'focused-source.mech')
(directory/'focused-extraction.json').write_text(json.dumps({
    'source_files':{path:hashlib.sha256((root/path).read_bytes()).hexdigest() for path in paths},
    'retained_definitions':sorted('.'.join(filter(None,key)) for key in required),
    'focused_sha256':hashlib.sha256(text.encode()).hexdigest()},indent=2)+'\n')
print(json.dumps({'bytes':len(text.encode()),'definitions':len(required),'original_bytes':sum((root/path).stat().st_size for path in paths)}))
