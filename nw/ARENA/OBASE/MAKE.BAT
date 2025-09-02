@echo off
secho d [
call compile.bat echodir.bat %1 %2 %3 %4
secho n ]

secho d [
call compile.bat makemod.bat %1 %2 %3 %4
secho n ]

wmake %1 %2 %3 %4
