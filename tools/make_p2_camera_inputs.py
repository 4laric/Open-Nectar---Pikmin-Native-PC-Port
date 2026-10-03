"""Publish genuine raw cave/ground source camera parameters in a fresh supplement.

Preserves and hashes every selected-parent role. Supplies no controller, viewport
or Scene grant; selected descriptor owner must merge the roles into a successor.
"""
import argparse
from pathlib import Path
from make_p2_system_inputs import generate as publish

SOURCES=(
 ("user/Nishimura/Camera/caveCameraParms.txt","p2-original/camera/caveCameraParms.txt",5797,"91284c53790a35d6fb4bde1c3bcbb8f7058d9792f35ff3966ecd4a331b6d71a4"),
 ("user/Nishimura/Camera/groundCameraParms.txt","p2-original/camera/groundCameraParms.txt",5799,"cd3b174aa8bc0f3900a915c7b6dede87d15b6e3efa9395f5bef2ab67cf056846"),
)
def generate(iso,selected,output):
    return publish(iso,selected,output,sources=SOURCES,kind="CAMERA")

if __name__=="__main__":
    p=argparse.ArgumentParser(description=__doc__)
    for name in ("iso","selected","output"):p.add_argument("--"+name,type=Path,required=True)
    a=p.parse_args();generate(a.iso,a.selected,a.output)
