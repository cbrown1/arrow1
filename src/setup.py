from setuptools import setup

__version__ = '2.0'

setup(
    name='arrow1', 
    version=__version__, 
    py_modules=['arrow1'],
    install_requires=[
        'scipy',
        'numpy'
    ]
)
