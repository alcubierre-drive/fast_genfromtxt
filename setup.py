from setuptools import setup

setup(
   name='fast_genfromtxt',
   version='v0.0.1',
   description='fast version of numpy\'s genfromtxt/savetxt for floats',
   long_description_content_type = 'text/markdown',
   packages=['fast_genfromtxt'],
   package_dir={'fast_genfromtxt': 'src'},
   package_data={'fast_genfromtxt': ['*.so', '*.py']},
   install_requires=['numpy>=1.13', 'setuptools>=30.3.0'],
)

