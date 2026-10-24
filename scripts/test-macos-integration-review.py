#!/usr/bin/env python3
"""Optional manual package integration review on fresh disposable native Macs.

Consumes separately identified finished package artifacts; never builds a sibling
repository and never alters the production default-full exact-three receipt gate.
No receipt forgetting, pre-existing product removal or synthetic live guard context.
"""
import argparse
import hashlib
import importlib.util
import itertools
import json
import os
import platform
from pathlib import Path
import plistlib
import re
import subprocess
import sys
import tempfile
import zipfile

ROOT=Path(__file__).resolve().parent.parent
PRODUCTS={'Anamorph':('0.9.10','Anmr'),'Anabasis':('0.2.17','Anbs')}
CHOICES=('vst3','au','app')
COMBINATIONS=tuple(','.join(c) for size in (1,2,3) for c in itertools.combinations(CHOICES,size))


def require(condition,message):
    if not condition: raise ValueError(message)


def validate_manifest(value):
    require(isinstance(value,dict) and value and set(value)<=set(PRODUCTS),'unknown product manifest')
    for product,item in value.items():
        require(item['repository']=='skyRolly/'+product,'wrong artifact repository')
        require(type(item['run_id']) is int and item['run_id']>0,'invalid artifact run ID')
        require(re.fullmatch('[0-9a-f]{40}',item['head_sha']) is not None,'invalid artifact source head')
        require(re.fullmatch('[0-9a-f]{64}',item['pkg_sha256']) is not None,'invalid package digest')
    return value


def command(*argv):
    result=subprocess.run(argv,stdout=subprocess.PIPE,stderr=subprocess.PIPE)
    require(result.returncode==0,'native/artifact command failed: '+' '.join(argv)+'\n'+result.stderr.decode(errors='replace'))
    return result.stdout


def validator(product):
    # The same full ownership/content/location checks apply to both products;
    # explicit family constants supply their independent identities. These are
    # pure functions: no sibling host guard is called or its environment faked.
    spec=importlib.util.spec_from_file_location('receipt_'+product,ROOT/'scripts/test-macos-receipts.py')
    module=importlib.util.module_from_spec(spec);spec.loader.exec_module(module)
    module.PRODUCT=product;module.BASE='tech.rolly.'+product.lower()
    module.CODE=PRODUCTS[product][1]
    module.COMPONENTS={key:(location,product+extension) for key,location,extension in
                       [('vst3','/Library/Audio/Plug-Ins/VST3','.vst3'),('au','/Library/Audio/Plug-Ins/Components','.component'),('app','/Applications','.app')]}
    return module


def receipt_set(): return set(command('/usr/sbin/pkgutil','--pkgs').decode().splitlines())


def snapshot(product):
    module=validator(product); ids=receipt_set(); records={}
    for key,(location,name) in module.COMPONENTS.items():
        identifier=module.BASE+'.'+key;bundle=Path(location)/name
        if identifier in ids:
            records[key]={'info':plistlib.loads(command('/usr/sbin/pkgutil','--pkg-info-plist',identifier)),
                          'files':command('/usr/sbin/pkgutil','--files',identifier).decode(),
                          'installed_payload':module.tree(bundle)}
        else: require(not bundle.exists() and not bundle.is_symlink(),'unreceipted existing bundle: '+str(bundle))
    return records


def download(product,item,destination):
    repo=item['repository']; run=json.loads(command('gh','api',f'repos/{repo}/actions/runs/{item["run_id"]}'))
    require(run['head_sha']==item['head_sha'],'artifact run source head differs')
    artifacts=json.loads(command('gh','api',f'repos/{repo}/actions/runs/{item["run_id"]}/artifacts?per_page=100'))['artifacts']
    selected={}
    for name,folder in [(product+'-macOS','staged'),(product+'-macOS-installer','package')]:
        matches=[a for a in artifacts if a['name']==name and not a['expired']]
        require(len(matches)==1,'one actual artifact required: '+name)
        artifact=matches[0]; archive=destination/(folder+'.zip')
        with archive.open('wb') as stream:
            result=subprocess.run(['gh','api',f'repos/{repo}/actions/artifacts/{artifact["id"]}/zip'],stdout=stream,stderr=subprocess.PIPE)
        require(result.returncode==0,'artifact archive download failed: '+result.stderr.decode(errors='replace'))
        digest=hashlib.sha256(archive.read_bytes()).hexdigest()
        require(artifact.get('digest')=='sha256:'+digest,'artifact archive digest differs from GitHub metadata')
        target=destination/folder;target.mkdir()
        with zipfile.ZipFile(archive) as zipped:
            for info in zipped.infolist():
                require(not Path(info.filename).is_absolute() and '..' not in Path(info.filename).parts,'unsafe artifact entry')
            zipped.extractall(target)
        selected[name]={'id':artifact['id'],'archive_sha256':digest,'run_id':item['run_id'],'head_sha':item['head_sha']}
    packages=list((destination/'package').glob(product+'-*-macOS.pkg'))
    require(len(packages)==1,'one actual package required')
    package=packages[0]
    require(hashlib.sha256(package.read_bytes()).hexdigest()==item['pkg_sha256'],'package bytes differ from independently identified digest')
    return package,destination/'staged',selected


def validate_installed(product,selected,staged,evidence):
    module=validator(product); records={}; ids=receipt_set()
    for key,(location,name) in module.COMPONENTS.items():
        identifier=module.BASE+'.'+key;bundle=Path(location)/name
        if key not in selected:
            require(identifier not in ids and not bundle.exists() and not bundle.is_symlink(),'unselected component installed: '+key)
            continue
        raw=command('/usr/sbin/pkgutil','--pkg-info-plist',identifier)
        files=command('/usr/sbin/pkgutil','--files',identifier).decode()
        (evidence/(product+'-'+key+'.plist')).write_bytes(raw)
        (evidence/(product+'-'+key+'.files.txt')).write_text(files)
        records[key]=module.validate_component(key,plistlib.loads(raw),files,bundle,staged/name,PRODUCTS[product][0])
    return records


def install(product,selected,package,staged,evidence):
    require(set(selected)<=set(CHOICES) and selected,'nonempty selection required')
    before=receipt_set(); before_products={name:snapshot(name) for name in PRODUCTS}
    require(not before_products[product],'existing product installation: refusing')
    base='tech.rolly.'+product.lower()
    require(not any(i==base or i.startswith(base+'.') for i in before),'existing product receipt: refusing')
    choices=command('/usr/sbin/installer','-showChoicesXML','-pkg',str(package),'-target','/')
    changes=command('/usr/sbin/installer','-showChoiceChangesXML','-pkg',str(package),'-target','/')
    (evidence/(product+'-choices.xml')).write_bytes(choices);(evidence/(product+'-choice-changes.xml')).write_bytes(changes)
    measured=plistlib.loads(changes)
    require({x['choiceIdentifier'] for x in measured if x.get('choiceAttribute')=='selected'}==set(CHOICES),'native choice IDs differ')
    request=[{'choiceIdentifier':key,'choiceAttribute':'selected','attributeSetting':int(key in selected)} for key in CHOICES]
    request_path=evidence/(product+'-requested-choices.plist');request_path.write_bytes(plistlib.dumps(request))
    preview=command('/usr/sbin/installer','-showChoicesAfterApplyingChangesXML',str(request_path),'-pkg',str(package),'-target','/')
    (evidence/(product+'-choice-preview.xml')).write_bytes(preview)
    preview_values={x['choiceIdentifier']:x['attributeSetting'] for x in plistlib.loads(preview) if x.get('choiceAttribute')=='selected'}
    require(preview_values=={key:int(key in selected) for key in CHOICES},'native choice preview differs')
    result=subprocess.run(['sudo','-n','/usr/sbin/installer','-pkg',str(package),'-target','/','-applyChoiceChangesXML',str(request_path)],stdout=subprocess.PIPE,stderr=subprocess.STDOUT)
    (evidence/(product+'-installer.log')).write_bytes(result.stdout)
    require(result.returncode==0,'native selected installation failed')
    after=receipt_set();require(after-before=={base+'.'+key for key in selected},'selection receipt additions differ')
    require(before<=after,'existing receipt disappeared')
    records=validate_installed(product,selected,staged,evidence)
    for sibling in set(PRODUCTS)-{product}: require(snapshot(sibling)==before_products[sibling],'sibling receipts/listings/payload changed')
    return {'selected':list(selected),'receipts_before':sorted(before),'receipts_after':sorted(after),
            'products_before':before_products,'products_after':{name:snapshot(name) for name in PRODUCTS},
            'components':records,'preexisting_receipts_preserved':True}


def self_test():
    require(len(COMBINATIONS)==7 and len(set(COMBINATIONS))==7,'selection inventory')
    value={'Anamorph':{'repository':'skyRolly/Anamorph','run_id':1,'head_sha':'a'*40,'pkg_sha256':'b'*64}}
    validate_manifest(value)
    for key,bad in [('repository','skyRolly/Anabasis'),('run_id',0),('head_sha','main'),('pkg_sha256','unidentified')]:
        altered=json.loads(json.dumps(value));altered['Anamorph'][key]=bad
        try:validate_manifest(altered)
        except ValueError:pass
        else:raise AssertionError('accepted bad '+key)
    for product in PRODUCTS:
        module=validator(product)
        require(module.BASE=='tech.rolly.'+product.lower() and module.CODE==PRODUCTS[product][1],'independent identity')
    # These fixtures cannot invoke installer/gh or authorize a live host.


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--self-test',action='store_true')
    parser.add_argument('--product',choices=PRODUCTS)
    parser.add_argument('--components',choices=COMBINATIONS)
    parser.add_argument('--order',choices=('Anamorph,Anabasis','Anabasis,Anamorph'))
    parser.add_argument('--report',type=Path)
    args=parser.parse_args()
    if args.self_test:self_test();return
    require(sys.platform=='darwin' and os.environ.get('GITHUB_ACTIONS')=='true'
            and os.environ.get('RUNNER_ENVIRONMENT')=='github-hosted'
            and os.environ.get('RUNNER_OS')=='macOS'
            and os.environ.get('GITHUB_REPOSITORY') in ('skyRolly/Anamorph','skyRolly/Anabasis'),
            'real integration installation requires an authorized fresh disposable hosted Mac')
    require(bool(args.order) != bool(args.product and args.components),'choose one selection or co-installation order')
    require(args.report is not None,'evidence report required')
    manifest=validate_manifest(json.loads(os.environ['PLATFORM_REVIEW_ARTIFACTS']))
    order=args.order.split(',') if args.order else [args.product]
    require(set(order)<=set(manifest),'independently identified artifacts missing')
    ids=receipt_set()
    require(all(not snapshot(product) and not any(i=='tech.rolly.'+product.lower() or
                i.startswith('tech.rolly.'+product.lower()+'.') for i in ids) for product in PRODUCTS),
            'fresh Mac with neither product installed required')
    args.report.parent.mkdir(parents=True,exist_ok=True); evidence=args.report.parent/'native-evidence';evidence.mkdir(exist_ok=True)
    report={'failure':None,'order':order,'artifact_inputs':manifest,'installs':{},'all_six_receipts_verified':False,
            'host_head':os.environ.get('GITHUB_SHA'),'host':{'macOS':platform.mac_ver()[0],'architecture':platform.machine(),'runner':os.environ.get('RUNNER_NAME')},'run_id':os.environ.get('GITHUB_RUN_ID'),'manual_GUI_DAW_verified':False}
    try:
        with tempfile.TemporaryDirectory(prefix='platform-native-integration-') as tmp:
            for product in order:
                destination=Path(tmp)/product;destination.mkdir()
                package,staged,artifacts=download(product,manifest[product],destination)
                selected=CHOICES if args.order else tuple(args.components.split(','))
                report['installs'][product]={'artifacts':artifacts,'pkg_sha256':hashlib.sha256(package.read_bytes()).hexdigest(),
                                             **install(product,selected,package,staged,evidence)}
            if args.order:
                ids=receipt_set(); expected={'tech.rolly.'+p.lower()+'.'+c for p in PRODUCTS for c in CHOICES}
                require(expected<=ids,'missing co-installed receipts')
                report['all_six_receipts_verified']=True
    except Exception as error:report['failure']=str(error);raise
    finally:args.report.write_text(json.dumps(report,indent=2)+'\n')

if __name__=='__main__':main()
