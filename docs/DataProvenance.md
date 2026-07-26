# Data provenance

## Исходный код

Опубликованный архив был предоставлен разработчиком Святославом Образцовым
(Suavik). Примерная дата архива `nw205PeopleWater.rar` — 18 января 1999 года.
Он не является release snapshot.

`License.txt` сообщает, что оригинальные файлы разработаны и предоставлены
Logos, и разрешает публикацию и использование исходных текстов программ и
данных с изменениями при обязательном указании происхождения Logos.

Проект сохраняет исходные credits и не приписывает современным участникам
авторство унаследованного кода или данных.

## Официальный retail-диск

Локально исследован образ:

```text
F:\Downloads\Русская рулетка II - Закрытые планеты\RR2.mdf
F:\Downloads\Русская рулетка II - Закрытые планеты\RR2.mds
```

Он был смонтирован как `G:`. Подтвержденные свойства:

- volume label `RR2`;
- CDFS;
- 6,782 файлов на всем диске;
- 6,768 файлов в `G:\nw`;
- дата записи filesystem 27 мая 1999 года;
- retail `nw.exe` размером 2,179,072 bytes;
- издательская и разработческая атрибуция Buka/Logos на обложке.

Образ, scans и retail files являются локальными preservation artifacts и не
должны автоматически коммититься или публиковаться.

## Текущая локальная установка

Исследован путь:

```text
E:\Games\The Next Worlds
```

Он не является чистым retail baseline:

- полный retail data set присутствует в корне;
- тот же data set продублирован в `E:\Games\The Next Worlds\nw`;
- добавлен dgVoodoo 2.73;
- изменены `game.cfg`, user config, saves и один briefing;
- registry `HDDir` и `CDDir` указывают на установленную директорию;
- для `nw.exe` задан AppCompat `HIGHDPIAWARE WINXPSP2`.

Основной установленный `nw.exe` совпадает с дисковым по SHA-256. Установка
полезна как user-state fixture, но не как clean retail fixture.

## User-generated/private artifacts

В установке обнаружены пользовательские `save0` и `save1`. Они полезны для
legacy save importer и regression tests, но по умолчанию остаются приватными.
В публичный test corpus может попасть только специально созданный или явно
разрешенный sanitized fixture.

Registry exports, WER reports и dumps также являются приватными, даже когда
они используются для диагностики.

## External patch candidate

`NWRUS.EXE` находится рядом с образом, но не установлен вместо retail EXE. Он:

- имеет размер 797,286 bytes;
- упакован UPX 0.59;
- имеет PE timestamp, не отражающий реальную дату;
- не сопровождается достаточным changelog/provenance.

До распаковки и сравнения он классифицируется как `UNKNOWN_EXTERNAL_PATCH`, а
не как release baseline.

## Third-party components

Оригинальный диск содержит:

- DirectX 5-era installer;
- Intel RSX libraries и setup tools;
- Buka/installer components.

Они не нужны современному порту и не должны включаться в release package.
Разрешение Logos на исходники/данные не следует автоматически трактовать как
разрешение на распространение всех third-party binaries.

## Publication classes

| Class | Public Git | Release package |
| --- | --- | --- |
| Original source under project license | Yes, with attribution | Yes |
| Modern source/tests/docs | Yes | Source as selected |
| Hash manifests without media | Yes | Optional |
| Retail scripts/data | After scope review | Prefer importer first |
| Models/textures/audio/video from CD | No by default | Import locally |
| DirectX/RSX installers | No | No |
| Retail EXE | No | No |
| User saves/dumps/registry | No | No |
| Example mod created for RR2NW | Yes | Yes |

## Required attribution

Каждый source и binary release должен:

- сохранять `License.txt`;
- указывать Logos как разработчика оригинальных файлов;
- указывать Buka как издателя оригинального русского retail-издания;
- отделять original credits от RR2NW contributors;
- перечислять licenses новых dependencies и assets.
