# Reference builds and automated evidence

## Назначение

Reference build нужен для сравнения поведения, но не является обязательной
первой ступенью modern port. Проект различает:

- **reference artifacts** — неизменяемые исходные данные, EXE, manifests и
  сохранения;
- **legacy rebuild** — попытка воспроизвести январский EXE старым toolchain;
- **modern build** — текущий продукт на CMake и современном компиляторе.

Modern build может начаться после фиксации reference artifacts, не дожидаясь
legacy rebuild.

Первый M0 baseline завершен 26 июля 2026 года. Публичная агрегированная сводка
и хеши приватных manifests находятся в
[`reference/reports/m0-baseline.json`](../reference/reports/m0-baseline.json),
а процесс воспроизведения — в
[`tools/reference/README.md`](../tools/reference/README.md).

Полный source-tree manifest относится к commit
`e5d7a96ad6ac42fc814876ae82c783dd24eaac5d` (`win10-stability-fixes`). Он
фиксирует рабочее patched-состояние, а не выдается за byte-identical январский
архив. Runtime `nw/OUTPUT`, использованный в source/retail diff, этим patch не
менялся.

## Зафиксированные локальные артефакты

Хеши относятся к исследованным 26 июля 2026 года локальным файлам. Пути не
являются требованиями к пользовательской установке.

| Artifact | SHA-256 |
| --- | --- |
| `RR2.mdf` | `8F3D3FFC236DB6A8809125D39B3D4E3C135865662D043762E0A2CA3318812BDC` |
| `RR2.mds` | `3FACCEC294F591E122AAD3D90B122896F85846387E166D3B778EC5C005D321D5` |
| retail `nw.exe` | `42F2FC3B632C58073307B1B95924C1EFC038B5B3879C7476E438336B5D497132` |
| external `NWRUS.EXE` | `30E6C9AA1A9B7575A287AA5B37D2A4032B1D1660FC15911BF6075547229DD149` |

Retail volume label: `RR2`. CDFS и retail EXE датированы 27 мая 1999 года.

`NWRUS.EXE` упакован UPX 0.59, имеет искусственный PE timestamp 1971 года и не
является reference EXE до выяснения происхождения и распаковки.

## M0 без legacy OS и ручного тестирования

Первая стадия выполняется на актуальной Windows:

1. Создать file manifest: normalized relative path, size, timestamp и SHA-256.
2. Проверить volume label и PE metadata.
3. Построить source/retail diff с raw и normalized-text сравнением.
4. Скопировать retail data в отдельный read-only fixture либо сформировать его
   из оригинального диска воспроизводимым скриптом.
5. Сохранить пользовательские saves/config отдельно от fixture.
6. Запускать EXE автоматическим harness с timeout и сбором exit code, WER и
   Event Log.
7. Не требовать ручного ввода, прохождения и визуальной приемки.

M0 считается завершенным по manifests и повторяемым отчетам, а не по тому,
можно ли долго играть в оригинальный EXE.

Текущий automated launch observation удержал retail EXE запущенным восемь
секунд без нового Application Error/WER event, после чего harness завершил
ровно запущенный PID. Это подтверждает работу harness и boot path, но не
является доказательством стабильности gameplay.

## Текущий DEP/PE факт

В Windows Event Log обнаружены повторяющиеся `0xc0000005` по RVA:

- `0x001dec8c`;
- `0x001decad`;
- `0x001decb7`.

Эти адреса являются инструкциями внутри PE section `DGROUP`. Section помечена
как `DATA`, но не как executable. На исследованной системе DEP работает в
режиме `OptOut`, поэтому выполнение таких инструкций является высоковероятной
причиной повторяющегося access violation.

Окончательное подтверждение требует full dump с exception information,
показывающей execute access violation.

### Допустимое лабораторное использование retail EXE

- Работать только с копией EXE и fixture.
- Для reference-run разрешается точечный PE section fix либо per-application
  DEP exception.
- Не отключать DEP глобально.
- Не включать измененный retail EXE в публичный релиз.
- Записать исходный и измененный хеш, способ модификации и rollback.

Modern build обязан работать с нормальными executable sections и не требовать
DEP exception.

## Legacy rebuild lane

Предполагаемый toolchain:

- Watcom C/C++ 10.6;
- Turbo Assembler/TASMX;
- `angel.exe`/`angelmasm.exe`;
- исторические DirectX import libraries;
- `designwr.lib` из `disgnwr.make`;
- исходные BAT/make/link response files.

Предпочтительный порядок:

1. Попытаться запускать 32-битные инструменты непосредственно на современной
   Windows.
2. Для единственного несовместимого DOS tool использовать DOSBox-X в scripted
   build mode.
3. Использовать обычную VM только при доказанном blocker.

PCem, 86Box, Windows 98 gameplay и ручное прохождение не входят в gate.

## Требуемый результат legacy rebuild

- полный stdout/stderr build log;
- точные версии и хеши инструментов;
- итоговый EXE и SHA-256;
- linker MAP;
- список локальных build-only corrections;
- автоматический launch smoke;
- отдельно vanilla и `win10-stability-fixes` варианты.

Watcom 11 не принимается как эквивалент Watcom 10.6 без behavioral evidence:
историческая записка сообщает, что он создавал неработающий EXE.

## Правила хранения

В публичный Git разрешены manifests, scripts, отчеты и документация. Не следует
добавлять:

- MDF/MDS/ISO и содержимое пользовательской установки;
- retail EXE и third-party installers;
- user saves и registry exports;
- дампы памяти;
- credentials и локальные абсолютные конфигурации;
- сам локальный handoff.
