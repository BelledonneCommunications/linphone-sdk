import os
import sys

# Get path where wheel was installed
base_dir = os.path.dirname(os.path.abspath(__file__))

sys.path.append(base_dir)

from pylinphone import *

grammars_dir = os.path.join(base_dir, 'share/')
Factory.get().top_resources_dir = grammars_dir

print(Core.get_version())