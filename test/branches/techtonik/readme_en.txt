
== Compile with Perl ==

To compile Far Encyclopedia using Perl make sure these folders are present:

+ enc     - empty output folder where the enc will be built
+ enc_eng - english source (dir name is important for scripts)
+ enc_rus - russian enc source
+ tools   - (for Perl scripts dir name is important)

Change directory to tools and issue:

perl tool.make_chm.pl

enc folder will be populated with .html and HtmlHelp .hh? project files
necessary to build compiled .chm with HtmlHelp Workshop (HCW) or hhc.exe
cmdline utility.

Perl scripts are maintained by Alex Yaroslavsky <trexinc@gmail.com>
-----------------------------------------------------------------------------

== Compile with Python ==

Output will be held at build/ directory, configured in tools/config.inc.py
Change directory to tools and launch:

make_chm.py

build/ folder will contain generated source files fro .chm that can be
compiled with hhc.exe commandline or HCW GUI utility.

Python scripts are maintained by techtonik <techtonik@gmail.com>
-----------------------------------------------------------------------------

== Other Python Tools ==

+ tools\untranslated.py
   - utility to locate untranslated russian strings

-----------------------------------------------------------------------------
