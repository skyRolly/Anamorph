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
import io
import copy
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
import xml.etree.ElementTree as ET

ROOT=Path(__file__).resolve().parent.parent
PRODUCTS={'Anamorph':'Anmr','Anabasis':'Anbs'}
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
    module.CODE=PRODUCTS[product]
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


def extract_artifact(archive, target):
    # Downloads always use a newly created private, empty destination. Do not
    # permit callers to reuse a tree containing link parents. ZipFile currently
    # extracts archive-encoded symlinks as regular files; verify containment too.
    require(target.is_dir() and not target.is_symlink() and not any(target.iterdir()),
            'artifact extraction requires a fresh empty directory')
    root=target.resolve()
    with zipfile.ZipFile(archive) as zipped:
        for info in zipped.infolist():
            path=Path(info.filename)
            require(not path.is_absolute() and '..' not in path.parts,'unsafe artifact entry')
            require((root/path).resolve().is_relative_to(root),'artifact entry escapes extraction root')
        zipped.extractall(root)
    require(all(item.resolve().is_relative_to(root) for item in root.rglob('*')),
            'extracted artifact escapes extraction root')


def bundle_metadata(declaration, info, name, base):
    require(declaration.get('id')==base and info.get('CFBundleIdentifier')==base,
            'wrong package/staged bundle identity')
    require(declaration.get('path','').removeprefix('./')==name,'wrong package bundle path')
    for key in ('CFBundleShortVersionString','CFBundleVersion'):
        require(isinstance(info.get(key),str) and bool(info[key]) and
                not any(ord(c)<32 for c in info[key]),'missing staged bundle version: '+key)
        require(declaration.get(key)==info[key],'package/staged bundle version differs: '+key)
    # Apple defines ShortVersionString as the marketing release and BundleVersion
    # as its build iteration. Compare like fields; the two need not be identical.
    return {'marketing_version':info['CFBundleShortVersionString'],'build_version':info['CFBundleVersion']}


def validate_package_metadata(product, distribution, components, staged):
    """Pure pre-install contract over native pkgutil-expanded XML and staged plists."""
    module=validator(product); expected={module.BASE+'.'+key:key for key in CHOICES}
    dist=ET.fromstring(distribution)
    require(dist.tag=='installer-gui-script','missing native Distribution')
    records={}
    for folder,xml in components:
        info=ET.fromstring(xml);identifier=info.get('identifier')
        require(info.tag=='pkg-info' and identifier in expected,'wrong component identity')
        require(identifier not in records,'duplicate component identity')
        key=expected[identifier];location,name=module.COMPONENTS[key]
        require(module.install_location(info.get('install-location'))==location,'wrong component install-location')
        bundles=info.findall('bundle');require(len(bundles)==1,'one component bundle required')
        plist=plistlib.loads((staged/name/'Contents/Info.plist').read_bytes())
        versions=bundle_metadata(bundles[0],plist,name,module.BASE)
        if key=='au':
            au=plist.get('AudioComponents',[])
            require(len(au)==1 and all(au[0].get(k)==v for k,v in
                    {'type':'aufx','subtype':PRODUCTS[product],'manufacturer':'RTec'}.items()),'wrong staged AU identity')
        version=info.get('version')
        # build-pkg.sh supplies the product marketing VERSION to pkgbuild.
        # CFBundleVersion may independently identify a build of that release.
        require(version==versions['marketing_version'],'component/marketing version differs')
        records[identifier]={'component':key,'package_version':version,'package_reference':folder,
                             'bundle_name':name,**versions}
    require(set(records)==set(expected),'missing component identity')
    references={}
    for ref in dist.findall('pkg-ref'):
        identifier=ref.get('id');require(identifier in expected,'wrong Distribution component identity')
        text=(ref.text or '').strip()
        if text:
            require(identifier not in references,'duplicate Distribution reference')
            record=records[identifier]
            require(text.removeprefix('#')==record['package_reference'] and
                    ref.get('version')==record['package_version'],'inconsistent Distribution reference')
            references[identifier]=text
        for bundle in ref.findall('bundle-version/bundle'):
            key=expected[identifier];name=module.COMPONENTS[key][1]
            bundle_metadata(bundle,plistlib.loads((staged/name/'Contents/Info.plist').read_bytes()),name,module.BASE)
    require(set(references)==set(expected),'missing Distribution reference')
    choices=dist.findall('choice')
    require(len(choices)==len(CHOICES) and {choice.get('id') for choice in choices}==set(CHOICES),'wrong Distribution choice identities')
    for choice in choices:
        refs=choice.findall('pkg-ref')
        require(len(refs)==1 and refs[0].get('id')==module.BASE+'.'+choice.get('id'),
                'inconsistent Distribution choice reference')
    return {record['component']:record for record in records.values()}


def inspect_package(product, package, staged, destination):
    expanded=destination/'expanded-package'
    command('/usr/sbin/pkgutil','--expand',str(package),str(expanded))
    components=[(str(path.parent.relative_to(expanded)),path.read_bytes())
                for path in sorted(expanded.rglob('PackageInfo'))]
    metadata=validate_package_metadata(product,(expanded/'Distribution').read_bytes(),components,staged)
    return {'components':metadata,'filename_hint':package.name,
            'metadata_source':'pkgutil --expand: PackageInfo and Distribution, checked against staged Info.plist'}


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
        extract_artifact(archive,target)
        selected[name]={'id':artifact['id'],'archive_sha256':digest,'run_id':item['run_id'],'head_sha':item['head_sha']}
    packages=list((destination/'package').glob(product+'-*-macOS.pkg'))
    require(len(packages)==1,'one actual package required')
    package=packages[0]
    require(hashlib.sha256(package.read_bytes()).hexdigest()==item['pkg_sha256'],'package bytes differ from independently identified digest')
    return package,destination/'staged',selected


def validate_installed(product,selected,staged,evidence,metadata):
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
        records[key]=module.validate_component(key,plistlib.loads(raw),files,bundle,staged/name,metadata['components'][key]['package_version'])
    return records


def install(product,selected,package,staged,evidence,metadata):
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
    records=validate_installed(product,selected,staged,evidence,metadata)
    for sibling in set(PRODUCTS)-{product}: require(snapshot(sibling)==before_products[sibling],'sibling receipts/listings/payload changed')
    return {'selected':list(selected),'receipts_before':sorted(before),'receipts_after':sorted(after),
            'products_before':before_products,'products_after':{name:snapshot(name) for name in PRODUCTS},
            'components':records,'preexisting_receipts_preserved':True}


def self_test():
    records=[]
    def rejects(call, reason):
        try:call()
        except ValueError as error:
            require(reason in str(error),'wrong rejection: '+str(error))
            records.append({'rejected':reason});return
        raise AssertionError('accepted invalid fixture: '+reason)
    require(len(COMBINATIONS)==7 and len(set(COMBINATIONS))==7,'selection inventory')
    value={'Anamorph':{'repository':'skyRolly/Anamorph','run_id':1,'head_sha':'a'*40,'pkg_sha256':'b'*64}}
    validate_manifest(value)
    for key,bad in [('repository','skyRolly/Anabasis'),('run_id',0),('head_sha','main'),('pkg_sha256','unidentified')]:
        altered=copy.deepcopy(value);altered['Anamorph'][key]=bad
        rejects(lambda:validate_manifest(altered),{'repository':'repository','run_id':'run ID','head_sha':'source head','pkg_sha256':'digest'}[key])
    # Inert XML/plist/file fixtures only. No installer, pkgutil or network call.
    with tempfile.TemporaryDirectory(prefix='integration-metadata-') as directory:
        root=Path(directory)
        for product,versions in [('Anamorph',('0.9.10','0.9.9')),('Anabasis',('0.2.17','0.2.18'))]:
            module=validator(product)
            for version in versions:
                staged=root/(product+'-'+version);staged.mkdir()
                dist=ET.Element('installer-gui-script');components=[]
                for key,(location,name) in module.COMPONENTS.items():
                    bundle=staged/name; (bundle/'Contents/MacOS').mkdir(parents=True)
                    info={'CFBundleIdentifier':module.BASE,'CFBundleShortVersionString':version,'CFBundleVersion':'123.4'}
                    if key=='au':info['AudioComponents']=[{'type':'aufx','subtype':PRODUCTS[product],'manufacturer':'RTec'}]
                    (bundle/'Contents/Info.plist').write_bytes(plistlib.dumps(info))
                    (bundle/'Contents/MacOS'/product).write_bytes(b'inert metadata fixture; never executed')
                    identifier=module.BASE+'.'+key;folder=product+key+'.pkg'
                    pkg=ET.Element('pkg-info',{'identifier':identifier,'version':version,'install-location':location})
                    decl={'id':module.BASE,'path':'./'+name,'CFBundleShortVersionString':version,'CFBundleVersion':'123.4'}
                    ET.SubElement(pkg,'bundle',decl)
                    components.append((folder,ET.tostring(pkg)))
                    ET.SubElement(ET.SubElement(dist,'choice',{'id':key}),'pkg-ref',{'id':identifier})
                    ET.SubElement(dist,'pkg-ref',{'id':identifier,'version':version}).text='#'+folder
                    ref=ET.SubElement(dist,'pkg-ref',{'id':identifier})
                    ET.SubElement(ET.SubElement(ref,'bundle-version'),'bundle',dict(decl,path=name))
                xml=ET.tostring(dist)
                def check(c=components,d=xml):return validate_package_metadata(product,d,c,staged)
                metadata=check()
                require(all(record['package_version']==version and record['build_version']=='123.4' for record in metadata.values()),'artifact-specific versions lost')
                records.append({'product':product,'package_version':version,'distinct_build_version':'123.4','valid':True})
                rejects(lambda:check(c=components[:-1]),'missing component identity')
                rejects(lambda:check(c=components+[components[0]]),'duplicate component identity')
                for field,bad,reason in [('identifier','tech.rolly.foreign.app','wrong component identity'),
                                         ('version','9.9.9','component/marketing version differs')]:
                    changed=ET.fromstring(components[0][1]);changed.set(field,bad)
                    rejects(lambda:check(c=[(components[0][0],ET.tostring(changed)),*components[1:]]),reason)
                for field in ('CFBundleShortVersionString','CFBundleVersion'):
                    changed=ET.fromstring(components[0][1]);changed.find('bundle').set(field,'9.9.9')
                    rejects(lambda:check(c=[(components[0][0],ET.tostring(changed)),*components[1:]]),'package/staged bundle version differs')
                changed=copy.deepcopy(dist);changed.find('pkg-ref').set('version','9.9.9')
                rejects(lambda:check(d=ET.tostring(changed)),'inconsistent Distribution reference')
                changed=copy.deepcopy(dist);changed.find('pkg-ref').text='#wrong.pkg'
                rejects(lambda:check(d=ET.tostring(changed)),'inconsistent Distribution reference')
                changed=copy.deepcopy(dist);changed.remove(changed.find('pkg-ref'))
                rejects(lambda:check(d=ET.tostring(changed)),'missing Distribution reference')
                changed=copy.deepcopy(dist);changed.append(copy.deepcopy(changed.find('pkg-ref')))
                rejects(lambda:check(d=ET.tostring(changed)),'duplicate Distribution reference')
                changed=copy.deepcopy(dist);changed.find('choice/pkg-ref').set('id',module.BASE+'.wrong')
                rejects(lambda:check(d=ET.tostring(changed)),'inconsistent Distribution choice reference')
                # Validate a real pure receipt function against expectations from
                # PackageInfo, never from the receipt being checked.
                key='app';location,name=module.COMPONENTS[key];bundle=staged/name
                receipt={'pkgid':module.BASE+'.'+key,'pkg-version':version,'volume':'/','install-location':location}
                listing='\n'.join(name+'/'+leaf for leaf in module.tree(bundle))+'\n'
                expected=metadata[key]['package_version']
                module.validate_component(key,receipt,listing,bundle,bundle,expected)
                rejects(lambda:module.validate_component(key,dict(receipt,**{'pkg-version':'9.9.9'}),listing,bundle,bundle,expected),'wrong receipt version')
        extraction=[]
        for index,spelling in enumerate(('../marker','inside/../marker','/marker',r'..\marker',r'C:\marker',r'\\server\share\marker','bundle/Contents/marker')):
            archive=io.BytesIO()
            with zipfile.ZipFile(archive,'w') as zipped:zipped.writestr(spelling,b'benign marker')
            target=root/('extract-'+str(index));target.mkdir();archive.seek(0)
            if spelling in ('../marker','inside/../marker','/marker'):
                rejects(lambda:extract_artifact(archive,target),'unsafe artifact entry');accepted=False
            else:extract_artifact(archive,target);accepted=True
            require(all(path.resolve().is_relative_to(target.resolve()) for path in target.rglob('*')),'fixture escaped')
            extraction.append({'spelling':spelling,'accepted':accepted,'resolved_inside_owned_root':True})
        archive=io.BytesIO()
        with zipfile.ZipFile(archive,'w') as zipped:
            link=zipfile.ZipInfo('encoded-link');link.create_system=3;link.external_attr=0o120777<<16
            zipped.writestr(link,'../outside-marker');zipped.writestr('normal-marker',b'benign')
        target=root/'encoded-link';target.mkdir();archive.seek(0);extract_artifact(archive,target)
        require((target/'encoded-link').is_file() and not (target/'encoded-link').is_symlink(),'review changed ZipFile link behavior')
        target=root/'link-parent';target.mkdir();(target/'parent').symlink_to(root,target_is_directory=True)
        archive.seek(0);rejects(lambda:extract_artifact(archive,target),'fresh empty directory')
    return {'metadata_cases':records,'extraction_cases':extraction,
            'archive_links_extracted_as_regular_files':True,'preexisting_link_parent_rejected':True,
            'installer_or_network_executed':False}


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--self-test',action='store_true')
    parser.add_argument('--product',choices=PRODUCTS)
    parser.add_argument('--components',choices=COMBINATIONS)
    parser.add_argument('--order',choices=('Anamorph,Anabasis','Anabasis,Anamorph'))
    parser.add_argument('--report',type=Path)
    args=parser.parse_args()
    if args.self_test:
        result=self_test()
        if args.report:
            args.report.parent.mkdir(parents=True,exist_ok=True)
            args.report.write_text(json.dumps(result,indent=2)+'\n')
        return
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
                metadata=inspect_package(product,package,staged,destination)
                selected=CHOICES if args.order else tuple(args.components.split(','))
                report['installs'][product]={'artifacts':artifacts,'pkg_sha256':hashlib.sha256(package.read_bytes()).hexdigest(),
                                             'verified_package_metadata':metadata, **install(product,selected,package,staged,evidence,metadata)}
            if args.order:
                ids=receipt_set(); expected={'tech.rolly.'+p.lower()+'.'+c for p in PRODUCTS for c in CHOICES}
                require(expected<=ids,'missing co-installed receipts')
                report['all_six_receipts_verified']=True
    except Exception as error:report['failure']=str(error);raise
    finally:args.report.write_text(json.dumps(report,indent=2)+'\n')

if __name__=='__main__':main()
