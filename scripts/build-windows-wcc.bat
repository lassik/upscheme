@setlocal
@prompt "+ "
@cd "%~dp0"\..
if not exist build-windows-x86-wcc mkdir build-windows-x86-wcc
@cd build-windows-x86-wcc
@echo Entering directory '%cd%'

@echo The WATCOM environment variable is set to '%WATCOM%'

wcc386 -q -wx ..\c\algo_color.c
wcc386 -q -wx ..\c\bitvector-ops.c
wcc386 -q -wx ..\c\bitvector.c
wcc386 -q -wx ..\c\buf.c
wcc386 -q -wx ..\c\builtins.c
wcc386 -q -wx ..\c\dump.c
wcc386 -q -wx ..\c\env_windows.c
wcc386 -q -wx ..\c\equalhash.c
wcc386 -q -wx ..\c\flisp.c
wcc386 -q -wx ..\c\hashing.c
wcc386 -q -wx ..\c\htable.c
wcc386 -q -wx ..\c\int2str.c
wcc386 -q -wx ..\c\ios.c
wcc386 -q -wx ..\c\iostream.c
wcc386 -q -wx ..\c\libraries.c
wcc386 -q -wx ..\c\lltinit.c
wcc386 -q -wx ..\c\os_windows.c
wcc386 -q -wx ..\c\ptrhash.c
wcc386 -q -wx ..\c\random.c
wcc386 -q -wx ..\c\string.c
wcc386 -q -wx ..\c\table.c
wcc386 -q -wx ..\c\text_ini.c
wcc386 -q -wx ..\c\time_windows.c
wcc386 -q -wx ..\c\utf8.c

wcc386 -q -wx ..\c\flmain.c

wlink op q name upscheme file algo_color, bitvector-ops, bitvector, buf, builtins, dump, env_windows, equalhash, flisp, hashing, htable, int2str, ios, iostream, libraries, lltinit, os_windows, ptrhash, random, string, table, time_windows, text_ini, utf8, flmain
