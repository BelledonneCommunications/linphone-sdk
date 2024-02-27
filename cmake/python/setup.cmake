from setuptools import setup, Distribution

class BinaryDistribution(Distribution):
    def has_ext_modules(foo):
        return True

setup(
    name="linphone",
    version="@WHEEL_LINPHONESDK_VERSION@",
    author="Belledonne Communications",
    author_email="info@belledonne-communications.com",
    description="A python wrapper for linphone library",
    long_description="A python wrapper for linphone library automatically generated using Cython",
    long_description_content_type="plain/text",
    url="https://linphone.org",
    license="GPLv3",
    packages=['linphone'],
    package_data={
        'linphone': ['*.so*', '*.dylib*', 'Frameworks/', 'share/belr/grammars/*'],
    },
    include_package_data=True,
    distclass=BinaryDistribution,
    classifiers=[
        'Intended Audience :: Developers',
        'License :: OSI Approved :: GNU General Public License v3 (GPLv3)',
        'Topic :: Communications :: Chat',
        'Topic :: Communications :: Conferencing',
        'Topic :: Communications :: Internet Phone',
        'Topic :: Communications :: Telephony',
    ],
)
