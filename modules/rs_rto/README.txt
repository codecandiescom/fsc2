Fsc3 is a new project that will (if things are going to plans)
supply C++ modules for controlling devices used in physics and
chemistry laboratories. It is intended to supply stand-alone
C++, C and Python modules for each device.

These modules are supposed to work under Linux (or, as far as
possible) UNIX based operating systems. No attempt (at least
from my side) will be made to support Windows - I neither
have the experience nor the inclination (and also no Windows
installed on any of my machines). Some modules may may only
need minor tweaks to get them to work under Windows - but I
can't tell for sure.

To compile the modules a working C++ (for at least C++11), is
required as well as a C compiler (supporting C99) and, for the
Python version, the boost::python library and the development
headers for Python. At the moment the Python bindings are for
Python 2.7 only, but I hope that this will change soon and
Python 3 will then also be supported.

This is a completely new project and at the moment there's only
two supported device, the Rhode&Schwarz SMB100A line of signal
generators and the RTO series of oscilloscopes by the same
company. This hopefuly will change in the near future.

Since many devices have an enormous amount of functionality, not
every part of that may be supported - I pick those parts that are
of importance to the users I know (some of who eveb paid me
for the effort;-)

All code is licensed under is under GPL version 3.

              9/8/2015         Jens Thoms Toerring
