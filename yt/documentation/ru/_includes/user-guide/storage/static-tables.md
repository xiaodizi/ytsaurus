# Статические таблицы 

В данном разделе содержится вводная информация о статических таблицах,  их типах и [схеме](#schema). 
Кроме статических таблиц, система {{product-name}} поддерживает динамические таблицы. Подробнее о видах и возможностях динамических таблиц можно прочитать в разделе [Динамические таблицы](../../../user-guide/dynamic-tables/overview.md).

## Общее описание { #common }

В системе {{product-name}} можно хранить различные [объекты](../../../user-guide/storage/objects.md): файлы, документы, таблицы. 

Таблицы бывают двух видов: статические и динамические. 
Статические таблицы подходят для хранения редко изменяемых данных, позволяют дописывать новые данные в конец таблиц.
Динамические таблицы подходят для часто изменяемых данных.

Статические таблицы — это классический тип таблиц в системе {{product-name}}. 
Физически такие таблицы разбиты на части ([чанки](../../../user-guide/storage/chunks.md)), каждая часть содержит фрагмент записей таблицы. 

Изменить данные статической таблицы можно только при:
- [слиянии](../../../user-guide/data-processing/operations/merge.md);
- [удалении](../../../user-guide/data-processing/operations/erase.md);
- добавлении записей в конец таблицы. 

Статические таблицы бывают [сортированные](#sorted_tables) и [несортированные](#unsorted_tables). 

### Сортированные таблицы { #sorted_tables }

Для сортированной таблицы известен набор неизменяемых ключевых колонок. 
Записи таблицы оказываются физически или логически упорядочены по ключу. Поэтому сортированные таблицы дают возможность эффективного поиска данных по ключу. 

Таблица сортированная — если в таблице есть хотя бы одна ключевая колонка, атрибут таблицы `sorted` равен `true`. Атрибут доступен только для чтения.

В схеме сортированных таблиц есть ключевые (сортированные) колонки. В схеме они помечены полем `sort_order`.

Список всех ключевых колонок доступен в атрибутах таблицы `key_columns` и `sorted_by`. Последовательность колонок определяется в схеме таблицы. Атрибуты доступны только для чтения.

{% note info "Примечание" %}

Записи, добавляемые в конец сортированной таблицы, не должны нарушать порядок сортировки.

{% endnote %}

### Несортированные таблицы { #unsorted_tables }

Для несортированных таблиц понятие ключа не определено, поэтому поиск данных по ключу неэффективен. Но возможно обращение по номерам строк.

{% note info "Примечание" %}

Добавление записей в конец несортированной таблицы лежит в основе многих способов загрузки данных в кластеры системы {{product-name}}. 
Чтобы эта операция была эффективна, учитывайте следующие особенноости:

 * Любой запрос на изменение обрабатывается мастер-сервером. Такая схема плохо масштабируется, поэтому не стоит делать более 100 записей в секунду.
 * Запись небольшого числа строк в одном запросе приводит к появлению мелких чанков, которые нагружают мастер-сервер большим объемом метаданных и делают чтение менее эффективным.

{% endnote %}

## Атрибуты { #attributes }

Любая статическая таблица имеет атрибуты, представленные в таблице:

| **Имя**       | **Тип**         | **Описание**                         | **Обязательный**      |
| ------------- | --------------- | ------------------------------------ |-----------------------|
| `sorted`      | `bool`          | Является ли таблица сортированной.   | Нет                   |
| `key_columns` | `array<string>` | Имена ключевых колонок.              | Да                    |
| `dynamic`     | `bool`          | Является ли таблица динамической.    | Нет                   |
| `schema`      | `TableSchema`   | Схема таблицы.                       | Нет                   |
| `data_weight` | `integer`       | "Логический" объем несжатых данных, записанных в таблицу. Зависит только от значений в ячейках таблицы и количества строк. Вычисляется как `row_count + sum(data_weight(value))` по всем значениям в ячейках таблицы. `data_weight` для значения зависит от физического типа значения: для `int64`, `uint64`, `double` — 8 байт; для `bool`, `null` — 1 байт; для `string` —  длина строки; для `any` — длина значения, сериализованного в binary YSON. | Да           |

Кроме того, все таблицы являются владельцами чанков. Следовательно, они получают соответствующие атрибуты, представленные в [таблице](../../../user-guide/storage/chunks.md#attributes).

## Схема статических таблиц { #schema }

Схема статической таблицы представляет из себя список описаний колонок. Подробное описание формата схемы приведено в разделе [Схема данных](../../../user-guide/storage/static-schema.md).

## Ограничения { #limitations }

На строки и схему таблицы наложен ряд ограничений по размеру и типу содержимого:

- Количество колонок в статической таблице не может быть больше 32768. Не рекомендуется использовать более тысячи колонок.
- Имя колонки должно содержать от 1 до 256 символов. Имя колонки может быть произвольной последовательностью байтов. Не может начинаться с зарезервированного системного префикса `@`.
- Максимальная длина значений типа `string` в статической таблице ограничивается через максимальный вес строки.
- Максимальный вес строки  — 128 мегабайт. Вес строки — сумма длин всех значений в данной строке в байтах. При этом длины значений считаются в зависимости от типа: 
  - `int64`, `uint64`, `double` — 8 байт; 
  - `boolean` — 1 байт; 
  - `string` — длина строки; 
  - `any` — длина структуры сериализованной в binary [yson](../../../user-guide/storage/yson.md), в байтах; 
  - `null` — 0 байт.
- Максимальный вес ключа в сортированной таблице — 256 килобайт. Ограничение по умолчанию — 16 килобайт. Вес ключа считается аналогично весу строки. 

  {% note warning "Внимание" %}

  Повышать лимит на вес ключа крайне не рекомендуется, так как есть риск переполнения памяти мастер-сервера. Изменение настройки `max_key_weight` допустимо только в качестве hotfix.

  {% endnote %}

  
