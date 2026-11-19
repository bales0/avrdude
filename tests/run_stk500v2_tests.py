"""Compile the real STK500v2 backend with simulated serial/USB/JTAG IO (MSYS2)."""
import argparse
import os
from pathlib import Path
import subprocess

parser=argparse.ArgumentParser()
parser.add_argument('--build-dir',type=Path,required=True,help='Existing AVRDUDE CMake/Ninja build')
parser.add_argument('--msys2-root',type=Path,default=Path('C:/msys64'))
parser.add_argument('--output-dir',type=Path)
args=parser.parse_args()
root=Path(__file__).resolve().parents[1]
build=args.build_dir.resolve()
output=(args.output_dir or build.parent/'transport-tests').resolve()
output.mkdir(parents=True,exist_ok=True)
tool_bin=args.msys2_root/'ucrt64/bin'
libraries=args.msys2_root/'ucrt64/lib'
exe=output/'stk500v2_transport.exe'
command=[str(tool_bin/'gcc.exe'),'-std=gnu11','-O1','-ffunction-sections','-fdata-sections',
         '-I'+str(build/'src'),'-I'+str(build),'-I'+str(root/'src'),'-I'+str(root),
         str(root/'tests/stk500v2_transport.c'),str(build/'src/libavrdude.a'),
         '-Wl,--gc-sections','-Wl,--wrap=jtagmkII_send','-Wl,--wrap=jtagmkII_recv',
         '-Wl,--wrap=jtag3_send','-Wl,--wrap=jtag3_recv','-L'+str(libraries),
         '-lm','-lelf','-lusb-1.0','-lhidapi','-lftdi1','-lreadline','-lserialport','-lncurses',
         '-lsetupapi','-lhid','-lws2_32','-o',str(exe)]
env=dict(os.environ)
env['PATH']=str(tool_bin)+os.pathsep+env.get('PATH','')
subprocess.run(command,check=True,env=env)
subprocess.run([str(exe)],check=True,env=env,timeout=15)
