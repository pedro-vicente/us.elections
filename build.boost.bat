@echo off
if not exist "build\boost_1_88_0" mkdir "build\boost_1_88_0"
cd ext\boost_1_88_0
call bootstrap.bat vc143
b2 --prefix=..\..\build\boost_1_88_0 --layout=versioned --toolset=msvc-14.3 address-model=64 architecture=x86 variant=release threading=multi link=static runtime-link=shared --build-dir=..\..\build\boost_1_88_0 --with-atomic --with-date_time --with-filesystem --with-program_options --with-regex --with-thread --with-chrono install
cd ..\..
