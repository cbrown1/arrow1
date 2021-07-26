from setuptools import setup


setup(name='arrow1',
    version='2.0',
    description='A very light python wrapper for arrow1, which plays and records sound using jack',
    url='http://github.com/cbrown1/arrow1',
    author='Andrzej Ciarkowski, Christopher Brown',
    author_email='cbrown1@pitt.edu',
    license='GPL3',
    packages=['arrow1'],
    zip_safe=False,
    install_requires=[
        'numpy',
    ],
      classifiers=[
        "License :: OSI Approved :: GNU General Public License v3 or later (GPLv3+)",
        "Operating System :: Microsoft :: Windows",
        "Operating System :: POSIX",
        "Operating System :: MacOS :: MacOS X",
        "Programming Language :: Python",
        "Programming Language :: Python :: 3.5",
        "Programming Language :: Python :: 3.6",
        "Programming Language :: Python :: 3.7",
        "Environment :: Console",
        "Development Status :: 5 - Production/Stable",
        "Intended Audience :: Developers",
        "Intended Audience :: Science/Research",
        "Topic :: Multimedia :: Sound/Audio",
        "Topic :: Scientific/Engineering",
        "Topic :: Scientific/Engineering :: Physics",
        "Natural Language :: English",
        ],
)
