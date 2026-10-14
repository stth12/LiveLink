"""Regenerate the checked-in Xcode project using only Python's standard library."""
from pathlib import Path
import hashlib

root = Path(__file__).resolve().parents[1] / 'ios'
def uid(name): return hashlib.sha256(name.encode()).hexdigest()[:24].upper()
def q(s): return '"' + s.replace('\\', '\\\\').replace('"', '\\"') + '"'
objects = []
def add(name, content):
    objects.append(f'{uid(name)} = {{ {content} }};')
    return uid(name)
sources = ['LiveLinkApp.swift', 'ContentView.swift', 'ReceiverModel.swift', 'AudioEngine.mm']
refs = []
builds = []
for source in sources + ['AudioEngine.h', 'LiveLink-Bridging-Header.h', 'Info.plist']:
    ext = source.rsplit('.', 1)[1]
    kind = {'swift':'sourcecode.swift','mm':'sourcecode.cpp.objcpp','h':'sourcecode.c.h','plist':'text.plist.xml'}[ext]
    ref = add(source, f'isa = PBXFileReference; lastKnownFileType = {kind}; path = {q("LiveLink/"+source)}; sourceTree = SOURCE_ROOT;')
    refs.append(ref)
    if source in sources: builds.append(add('build-'+source, f'isa = PBXBuildFile; fileRef = {ref};'))
asset = add('assets', 'isa = PBXFileReference; lastKnownFileType = folder.assetcatalog; path = LiveLink/Assets.xcassets; sourceTree = SOURCE_ROOT;')
refs.append(asset)
assetbuild = add('assetbuild', f'isa = PBXBuildFile; fileRef = {asset};')
product = add('product','isa = PBXFileReference; explicitFileType = wrapper.application; path = LiveLink.app; sourceTree = BUILT_PRODUCTS_DIR;')
products = add('products',f'isa = PBXGroup; children = ({product},); name = Products; sourceTree = "<group>";')
group = add('group',f'isa = PBXGroup; children = ({",".join(refs)},{products},); sourceTree = "<group>";')
sourcephase = add('sources',f'isa = PBXSourcesBuildPhase; buildActionMask = 2147483647; files = ({",".join(builds)},); runOnlyForDeploymentPostprocessing = 0;')
resourcephase = add('resources',f'isa = PBXResourcesBuildPhase; buildActionMask = 2147483647; files = ({assetbuild},); runOnlyForDeploymentPostprocessing = 0;')
frameworkrefs, frameworkbuilds = [], []
for name in ['AudioToolbox', 'AVFAudio', 'Network', 'UIKit']:
    ref = add('fw-'+name, f'isa = PBXFileReference; lastKnownFileType = wrapper.framework; name = {name}.framework; path = System/Library/Frameworks/{name}.framework; sourceTree = SDKROOT;')
    frameworkbuilds.append(add('fwbuild-'+name,f'isa = PBXBuildFile; fileRef = {ref};'))
frameworkphase = add('frameworks',f'isa = PBXFrameworksBuildPhase; buildActionMask = 2147483647; files = ({",".join(frameworkbuilds)},); runOnlyForDeploymentPostprocessing = 0;')
configs = []
projectconfigs = []
for name in ['Debug','Release']:
    settings = '''PRODUCT_NAME = LiveLink; PRODUCT_BUNDLE_IDENTIFIER = audio.livelink.receiver;
    INFOPLIST_FILE = LiveLink/Info.plist; SWIFT_VERSION = 5.0; IPHONEOS_DEPLOYMENT_TARGET = 16.0;
    SDKROOT = iphoneos; TARGETED_DEVICE_FAMILY = "1"; CODE_SIGN_STYLE = Automatic;
    SWIFT_OBJC_BRIDGING_HEADER = "LiveLink/LiveLink-Bridging-Header.h";
    CLANG_CXX_LANGUAGE_STANDARD = "c++20"; CLANG_CXX_LIBRARY = "libc++";
    HEADER_SEARCH_PATHS = ("$(inherited)", "$(SRCROOT)/..");
    ASSETCATALOG_COMPILER_APPICON_NAME = AppIcon; ENABLE_BITCODE = NO;
    SWIFT_OPTIMIZATION_LEVEL = ''' + ('"-Onone";' if name=='Debug' else '"-O";')
    configs.append(add('config-'+name,f'isa = XCBuildConfiguration; buildSettings = {{ {settings} }}; name = {name};'))
    projectconfigs.append(add('projectconfig-'+name,f'isa = XCBuildConfiguration; buildSettings = {{ CLANG_ENABLE_MODULES = YES; }}; name = {name};'))
configlist = add('configlist',f'isa = XCConfigurationList; buildConfigurations = ({",".join(configs)},); defaultConfigurationIsVisible = 0; defaultConfigurationName = Release;')
projectconfiglist = add('projectconfiglist',f'isa = XCConfigurationList; buildConfigurations = ({",".join(projectconfigs)},); defaultConfigurationIsVisible = 0; defaultConfigurationName = Release;')
target = add('target',f'isa = PBXNativeTarget; buildConfigurationList = {configlist}; buildPhases = ({sourcephase},{frameworkphase},{resourcephase},); buildRules = (); dependencies = (); name = LiveLink; productName = LiveLink; productReference = {product}; productType = "com.apple.product-type.application";')
project = add('project',f'isa = PBXProject; attributes = {{ LastUpgradeCheck = 1600; }}; buildConfigurationList = {projectconfiglist}; compatibilityVersion = "Xcode 14.0"; developmentRegion = en; knownRegions = (en,Base,); mainGroup = {group}; productRefGroup = {products}; projectDirPath = ""; projectRoot = ""; targets = ({target},);')
folder = root/'LiveLink.xcodeproj'
folder.mkdir(parents=True,exist_ok=True)
(folder/'project.pbxproj').write_text('// !$*UTF8*$!\n{ archiveVersion = 1; classes = {}; objectVersion = 56; objects = {\n'+'\n'.join(objects)+f'\n}}; rootObject = {project}; }}\n')
schemes = folder/'xcshareddata/xcschemes'
schemes.mkdir(parents=True,exist_ok=True)
(schemes/'LiveLink.xcscheme').write_text(f'''<?xml version="1.0" encoding="UTF-8"?>
<Scheme LastUpgradeVersion="1600" version="1.3">
<BuildAction parallelizeBuildables="YES" buildImplicitDependencies="YES"><BuildActionEntries><BuildActionEntry buildForTesting="YES" buildForRunning="YES" buildForProfiling="YES" buildForArchiving="YES" buildForAnalyzing="YES"><BuildableReference BuildableIdentifier="primary" BlueprintIdentifier="{target}" BuildableName="LiveLink.app" BlueprintName="LiveLink" ReferencedContainer="container:LiveLink.xcodeproj"/></BuildActionEntry></BuildActionEntries></BuildAction>
<LaunchAction buildConfiguration="Debug" useCustomWorkingDirectory="NO" ignoresPersistentStateOnLaunch="NO" debugDocumentVersioning="YES" allowLocationSimulation="YES"><BuildableProductRunnable runnableDebuggingMode="0"><BuildableReference BuildableIdentifier="primary" BlueprintIdentifier="{target}" BuildableName="LiveLink.app" BlueprintName="LiveLink" ReferencedContainer="container:LiveLink.xcodeproj"/></BuildableProductRunnable></LaunchAction>
<ArchiveAction buildConfiguration="Release" revealArchiveInOrganizer="YES"/>
</Scheme>''')
print('Generated', folder)
