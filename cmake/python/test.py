import os
from pylinphone import linphone

print(linphone.Core.get_version())

print(linphone.Factory.get().top_resources_dir)
linphone.Factory.get().top_resources_dir = os.path.dirname(os.path.abspath(linphone.__file__)) + "/share/" 
print(linphone.Factory.get().top_resources_dir)