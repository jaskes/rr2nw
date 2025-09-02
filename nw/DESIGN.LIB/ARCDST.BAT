@echo off
if '%1==' goto err
if exist %1.ZIP goto exist
pkzip -P %1.ZIP @src\arcdst.rsp
goto end
:exist
echo File %1.ZIP already exists
goto end
:err
echo Usage: arc arcname
:end
