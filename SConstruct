#!/usr/bin/python
# vim: set fileencoding=utf-8 :


import SCons
import SCons.Script as scons
from distutils import sysconfig, version
import os, sys, re, platform

EnsurePythonVersion(2, 5)
EnsureSConsVersion(2, 0)

colors = {}
colors['cyan'] = '\033[96m'
colors['purple'] = '\033[95m'
colors['blue'] = '\033[94m'
colors['green'] = '\033[92m'
colors['yellow'] = '\033[93m'
colors['red'] = '\033[91m'
colors['end'] = '\033[0m'

#If the output is not a terminal, remove the colors
if not sys.stdout.isatty():
    for key, value in colors.iteritems():
        colors[key] = ''

env = Environment()

AddOption(
    "--verbose",
    action="store_true",
    dest="verbose_flag",
    default=False,
    help="verbose output"
)

if not GetOption("verbose_flag"):
    env["CXXCOMSTR"] = \
            '%(blue)sCompiling%(purple)s: %(yellow)s$SOURCE%(end)s' % colors,
    env["CCCOMSTR"] = \
            '%(blue)sCompiling%(purple)s: %(yellow)s$SOURCE%(end)s' % colors,
    env["SHCCCOMSTR"] = \
            '%(blue)sCompiling shared%(purple)s: %(yellow)s$SOURCE%(end)s' % colors,
    env["SHCXXCOMSTR"] = \
            '%(blue)sCompiling shared%(purple)s: %(yellow)s$SOURCE%(end)s' % colors,
    env["ARCOMSTR"] = \
            '%(red)sLinking Static Library%(purple)s: %(yellow)s$TARGET%(end)s' % colors,
    env["RANLIBCOMSTR"] = \
            '%(red)sRanlib Library%(purple)s: %(yellow)s$TARGET%(end)s' % colors,
    env["SHLINKCOMSTR"] = \
            '%(red)sLinking Shared Library%(purple)s: %(yellow)s$TARGET%(end)s' % colors,
    env["LINKCOMSTR"] = \
            '%(red)sLinking Program%(purple)s: %(yellow)s$TARGET%(end)s' % colors,
    env["INSTALLSTR"] = \
            '%(green)sInstalling%(purple)s: %(yellow)s$SOURCE%(purple)s => %(yellow)s$TARGET%(end)s' % colors

AddOption(
    '--prefix',
    dest='prefix',
    type='string',
    nargs=1,
    action='store',
    metavar='DIR',
    default='/usr',
    help='installation prefix'
)
prefix = GetOption('prefix')

AddOption(
    '--bindir',
    dest='bindir',
    type='string',
    nargs=1,
    action='store',
    metavar='DIR',
    default='%s/bin' % prefix,
    help='installation binary files prefix'
)

bindir = GetOption('bindir')

# below three variables are not yet used
libdir = "%s/lib" % prefix
includedir = "%s/include" % prefix
datadir = "%s/share" % prefix

# Configuration:

checked_packages = dict()
def CheckPKGConfig(context, version):
    context.Message( 'Checking for pkg-config... ' )
    ret = context.TryAction('pkg-config --atleast-pkgconfig-version=%s' % version)[0]
    context.Result( ret )
    return ret

def CheckPKG(context, name):
    context.Message( 'Checking for %s... ' % name )
    m = re.match(r"(\w+)(?:-[\d.]+)?(?: (?:\=|\<|\>) \S+)?", name)
    key = m.group(1)
    if key in checked_packages:
        context.Result(checked_packages[key])
        return checked_packages[key]

    ret = context.TryAction('pkg-config --exists \'%s\'' % name)[0]

    context.sconf.config_h_text += "\n"
    context.sconf.Define(
        'HAVE_%s' % re.sub(r"\W",'_',key.upper()),
        1,
        "Define to 1 if you have the `%(name)s' package installed"
        % { 'name': name }
    )

    depends = os.popen('pkg-config --print-requires \'%s\'' %
                       name).readline()

    depends = re.findall(r"([-.\w]+(?: (?:\=|\<|\>) \S+)?)", depends)
    context.Result( ret )
    for dep in depends:
        ret &= context.sconf.CheckPKG(dep.strip())
        checked_packages[key] = ret
    
    return ret

conf = Configure(
    env,
    config_h="config.h",
    custom_tests = {
        'CheckPKGConfig' : CheckPKGConfig,
        'CheckPKG' : CheckPKG
    }
)


if not env.GetOption('clean') and not env.GetOption('help'):
    if 'CC' in os.environ:
        env.Replace(CC = os.environ['CC'])
        print(">> Using compiler " + os.environ['CC'])

    if 'CFLAGS' in os.environ:
        env.Replace(CFLAGS = os.environ['CFLAGS'])
        print(">> Appending custom build flags : " + os.environ['CFLAGS'])

    if 'LDFLAGS' in os.environ:
        env.Append(LINKFLAGS = os.environ['LDFLAGS'])
        print(">> Appending custom link flags : " + os.environ['LDFLAGS'])


    if not conf.CheckPKGConfig('0.15.0'):
        print 'pkg-config >= 0.15.0 not found.'
        Exit(1)
    
    if not conf.CheckPKG('gupnp-1.0'):
        Exit(1)
    
    if not conf.CheckLib('boost_regex-mt'):
        Exit(1)

    if not conf.CheckLib('boost_thread-mt'):
        Exit(1)

    if not conf.CheckLib('boost_program_options-mt'):
        Exit(1)


env = conf.Finish()

env.Append(CCFLAGS='-Wall -g -O3')
env.Append(CPPDEFINES=['_GNU_SOURCE', ('_FILE_OFFSET_BITS','64'), '_REENTRANT',
                       'HAVE_CONFIG_H'])


env.ParseConfig("pkg-config --cflags --libs gupnp-1.0")
sources = [ 'main.cpp', 'igd.cpp' ]

bubba_upnp = env.Program(target = "bubba-upnp", source = sources)

env.Install(bindir, [bubba_upnp])

env.Alias('install', bindir)
