@echo off
if '%1==' goto err
md %1
md %1\h
md %1\wc
copy h\stdtypes.h %1\h
copy h\debugext.h %1\h
copy h\filesys.h %1\h
copy h\mathlib.h %1\h
copy wc\design*.lib %1\wc
goto end
:err
echo Usage: install dir
:end
