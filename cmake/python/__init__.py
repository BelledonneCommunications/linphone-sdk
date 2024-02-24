import os
import sys

# Get path where wheel was installed (where this file is located).
base_dir = os.path.dirname(os.path.abspath(__file__))

# This allows loading the shared library containing the python wrapper module.
sys.path.append(base_dir)
from pylinphone import *

# This is required so the Factory knows where the grammar libraries are available.
# It will prevent the Core from aborting when created due to grammar not being found.
grammars_dir = os.path.join(base_dir, 'share/')
Factory.get().top_resources_dir = grammars_dir