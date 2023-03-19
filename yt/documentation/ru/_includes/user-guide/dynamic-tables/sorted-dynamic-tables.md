# Сортированные динамические таблицы

В данном разделе описано устройство сортированных динамических таблиц и действия, которые можно выполнять с данным видом таблиц.

## Модель данных { #model }

Каждая сортированная динамическая таблица — набор строк, упорядоченных по ключу. Как и в случае статических таблиц, ключ может быть композитным, то есть состоящим из нескольких колонок. В отличие от статических таблиц, ключ в динамической сортированной таблице уникален. 

Динамические сортированные таблицы должны быть строго схематизированы, то есть все имена колонок и их типы должны быть заранее указаны в схеме.

## Поддерживаемые операции { #methods }

### Создание

Для создания динамической сортированной таблицы необходимо выполнить команду `create table`, указав в атрибутах схему и настройку `dynamic=True`. Cхема должна соответствовать схеме сортированной таблицы. 

CLI
```bash
yt create table //path/to/table --attributes \
'{dynamic=%true;schema=[{name=key;type=string;sort_order=ascending}; {name=value;type=string}]}' 
```

{% note warning "Внимание" %}

Важно указать в схеме таблицы хотя бы одну ключевую колонку. Если этого не сделать, таблица будет успешно создана, но окажется не сортированной, а [упорядоченной](../../../user-guide/dynamic-tables/ordered-dynamic-tables.md). При этом для таблицы будет работать большинство типов select-запросов. Но все такие запросы будут сводиться к full scan данных.

{% endnote %}

### Изменение схемы и типа таблицы

Поменять схему уже существующей динамической таблицы можно с помощью команды `alter-table`. Для успешного выполнения команды таблица должна быть [смонтирована](../../../user-guide/dynamic-tables/overview.md#mount_table), а новая схема — совместима со старой. При этом никаких изменений в записанных на диск данных не происходит, поскольку старые данные удовлетворяют новой схеме. 

С помощью `alter-table` можно из динамической таблицы сделать статическую. Подробнее можно прочитать в разделе [MapReduce по динамическим таблицам](../../../user-guide/dynamic-tables/mapreduce.md#convert_table).

### Чтение строки

Клиент может выполнять чтение строк по заданному ключу методом `lookup`. Для этого необходимо указать имя таблицы, временную метку, определяющую срез читаемых данных (`<= t`), а также имена интересующих колонок. Данный запрос является точечным, то есть требует указания всех компонент ключа. Существует разновидность вызовов API, позволяющих читать несколько строк по разным ключам за один запрос.

При чтении в пределах атомарной транзакции будет использоваться временная метка старта транзакции. Можно также указать конкретное числовое значение. Например, приближенно отвечающее физическому моменту времени, а также одно из двух специальных значений:

- `sync_last_committed` — следует прочитать самую свежую версию, гарантированно содержащую все изменения, выполненные в уже закоммиченных транзакциях;
- `async_last_committed` — следует прочитать по возможности последнюю версию, но разрешается вернуть данные с небольшим неспецифицированным, типично в пределах десятков миллисекунд, отставанием.

Метка `async_last_committed` может быть использована в тех случаях, когда консистентность чтения не требуется. Данный режим может работать быстрее при наличии конкуренции между чтениями и двухфазными коммитами. При двухфазном коммите строки таблицы блокируются специальным образом до тех пор, пока для транзакции не началась вторая стадия и выбрана метка коммита. Читатели, желающие получить самые последние данные, вынуждены ждать окончания первой фазы, так как до этого неизвестно, будет ли транзакция успешной.

Обе метки `sync_last_committed`, `async_last_committed` не гарантируют глобального консистентного среза. Данные, которые увидит читающий запрос, могут отвечать разным моментам времени как на уровне целых строк, так и на уровне отдельных колонок строк. Для консистентного чтения по всей таблице или для набора таблиц необходимо указывать конкретную временную метку.

Для таблицы с неатомарными изменениями режимы `sync_last_committed` и `async_last_committed` эквивалентны, так как двухфазного коммита не происходит.

### Выполнение запроса

Система понимает SQL-подобный диалект, с помощью которого можно производить выборки и агрегацию по объемам данных в миллионы строк в режиме реального времени. Как и для операции чтения строк по ключу в запросе можно указать временную метку, относительно которой запрос следует выполнить.

Поскольку данные в системе фактически являются сортированными по набору ключевых колонок, при исполнении система использует данное свойство для сокращения объема прочитанного. В частности, пользовательский запрос анализируется и из него выводятся `key ranges` — диапазоны в пространстве ключей, объединение которых покрывает всю область поиска. Это позволяет выполнять эффективный `range scan` одним select-запросом. Подробнее можно прочитать в разделе [Язык запросов](../../../user-guide/dynamic-tables/dyn-query-language.md).

При построении запроса, следует помнить, что если система не сможет вывести нетривиальные `key ranges`, то произойдет `full scan` данных. `Full scan` случится, например, если в качестве ключевых колонок указать `key1, key2`, а в запросе задать фильтрацию лишь по `key2`.

### Запись строки { #insert_rows }

Клиент может выполнить запись данных методом `insert_rows` в пределах активной транзакции. Для этого он должен сообщить записываемые строки. В каждой такой строке должны присутствовать все ключевые поля. Часть полей данных из указанных в схеме может отсутствовать.

Семантически если строки с указанным ключом в таблице нет, то она появляется. Если же строка с таким ключом уже есть, то происходит перезапись части колонок.

При указании части полей существует 2 режима: 

- `overwrite` (по умолчанию) — все неуказанные поля обновляют свои значения на `null`; 
- `update` — включается опцией `update == true`. В таком случае сохранится предыдущее значение. В этом режиме необходимо передать все колонки, помеченные атрибутом `required`.

{% note info "Примечание" %}

При чтении и выполнении SQL запроса видны данные только на момент начала транзакции. Изменения, записанные в пределах той же транзакции, для чтения недоступны.

{% endnote %}

### Удаление строки

В пределах транзакции клиент может удалить строку или набор строк, сообщив соответствующие ключи.

Семантически если строка с указанным ключом присутствовала в таблице, то она будет удалена. Если же строки не было, то никаких изменений не наступит.

Как и в большинстве MVCC-систем, удаление в системе {{product-name}} сводится к записи строки без данных, но со специальным специальным маркером `tombstone`, сигнализирующим об удалении. Это означает, что освобождение дискового пространства от удаленных строк происходит не сразу, а отложенным образом. Также удаление строк не вызывает немедленного ускорения при чтении. Поскольку при чтении происходит слияние данных, оно замедляется.

### Блокировка строки

Клиент может заблокировать строки в пределах транзакции. Блокировка дает гарантию того, что строка в течение текущей транзакции не будет изменена в других транзакциях. Одну строку можно заблокировать сразу из нескольких транзакций. Можно указывать отдельные `lock` группы колонок, которые будут заблокированы, а также режим блокировки `weak` или `strong`.

### Удаление старых данных (TTL) { #remove_old_data }

В процессе слияния чанков таблета часть данных может быть признана устаревшей и удалена. 

{% note info "Примечание" %}

Удаление строк в транзакции фактически лишь записывает специальный маркер, но не освобождает память.

{% endnote %}

Значения атрибутов `min_data_versions`, `max_data_versions`, `min_data_ttl`, `max_data_ttl` показывают, можно ли удалить данные. Значение может быть удалено, если одновременно выполнено два условия:

- нет ни одного запрета удалять данное значение;
- есть хотя бы одно разрешение удалить данное значение.

Чтобы понять, для каких значений существуют разрешения и запреты и какие типы они имеют, можно мысленно отсортировать все значения в данной строке и данной колонке таблицы: (`t1, v1`), (`t2, v2`), ..., (`tn, vn`), где `ti` — временные метки, а `vi` — сами значения. Временные метки считаются упорядоченными по убыванию. Кроме того,  команда удаления строки таблицы порождает специальное значение для всех ее колонок: семантически оно не равно `null`, так как, после записи `null` в строки, их невозможно удалить. Тогда правила удаления таковы:

- первые `min_data_versions` значений нельзя удалять по соображениям числа версий;
- значения, записанные менее чем `min_data_ttl` до текущего момента нельзя удалять по соображениям времени;
- значения, следующие за первыми `max_data_versions`, можно удалять по соображениям числа версий;
- значения, записанные более давно чем `max_data_ttl` от текущего момента, можно удалять по соображениям времени.

Настройки по умолчанию: 

- min_data_versions = 1;
- max_data_versions = 1;
- min_data_ttl = 1800000 (30 min);
- max_data_ttl = 1800000 (30 min). 

По умолчанию хотя бы одно — последнее — значение будет сохраняться всегда, как и все значения, записанные за последние 30 минут. При этом ограничивается время, на протяжении которого транзакция может оставаться консистентной, система не допускает длинные транзакции.

Используя перечисленные параметры, можно строить гибкие политики хранения. Например, `min_data_versions = 0`, `max_data_versions = 1`, `min_data_ttl = 0`, `max_data_ttl = 86400000 (1 day)` разрешают удалять любые данные старше одного дня, сохраняя за последний день только одну версию.

{% note info "Примечание" %}

Указанные параметры дают системе возможность удалять данные, но не принуждают ее к этому. Операция слияния чанков и удаления данных является фоновой.

{% endnote %}

Если необходимо принудительно очистить данные, воспользуйтесь атрибутом `forced_compaction_revision`:

```bash
yt set //table/@forced_compaction_revision 1; yt remount-table //table 
```

Приведённый набор команд запускает компактификацию всех данных, записанных до текущего момента. Таким образом, будут удалены как лишние версии-дубликаты, так и логически удаленные данные. Данная операция создает моментальную нагрузку на кластер, которая зависит от объема многоверсионных данных, поэтому данная операция считается административным вмешательством. 

{% note warning "Внимание" %}

Установка `forced_compaction_revision` вызывает сильную нагрузку на кластер. Не рекомендуется использовать данный атрибут без особой необходимости и понимания последствий.

{% endnote %}

Когда в таблицу добавляются свежие записи по в среднем возрастающим ключам, а старые данные при этом удаляются также по в среднем возрастающим ключам. В конце таблицы возникают партиции c данными, для которых есть tombstones. Данные партиции не будут сжиматься, пока количество чанков в них мало. Размер таблицы, хранимый на диске, постоянно растет, хотя количество данных в ней останется на постоянном уровне. Возможное решение — указать параметр `auto_compaction_period`, задающий периодичность, с которой партции будут форсированно компактифицироваться.

## Агрегирующие колонки { #aggr_columns }

В случае, если сценарий работы с данными подразумевает постоянное прибавление дельт к значениям, уже записанным в таблице, стоит воспользоваться агрегирующими колонками. В схеме для колонки указывается атрибут `aggregate=sum` или другая агрегирующая функция. Далее можно делать запись в такую колонку с семантикой прибавления к значению, уже записанному в таблице.

Чтения старого значения не происходит, в таблицу записывается только дельта. Реальное суммирование происходит при чтении. Значения в таблицах хранятся вместе с временной меткой. При чтении из агрегирующей колонки значения, соответствующие одному ключу и одной колонке, но с разными временными метками, суммируются. Для оптимизации, на стадии компактификации старые данные в агрегирующей колонке суммируются, и в таблице остаётся только одно значение, соответствующее их сумме.

Поддерживаются следующие агрегирующие функции: `sum`, `min`, `max` или [`first`](*first).

По умолчанию происходит перезапись того, что находится в таблице. Чтобы записать дельту, в команде записи необходимо указать опцию `aggregate=true`.

## Построчный кеш { #lookup_cache }

Построчный кеш представляет собой функциональность, аналогичную memcached. Он будет полезен, если таблица не помещается в оперативную память, но при этом существует профиль чтения данных по ключам (lookup-запросы), попадающим в некоторое редко меняющееся множество, которое помещается в память).

Для использования кеша нужно указать в атрибутах таблицы количество строк на таблет, которое нужно кешировать, а в запросах, удовлетворяющих профилю чтения, указать опцию, которая разрешает использовать кеш. Опция в запросе нужна, чтобы отделить запросы, которые могут читать всю таблицу и удалять данные из кеша.

Атрибут таблицы: `lookup_cache_rows_per_tablet`

Опция lookup-запроса: `use_lookup_cache`

Для определения количества кешируемых строк на таблет можно воспользоваться следующими соотношениями:
- если известен размер working set, который в основном читается из таблицы, поделите его размер на количество таблетов.
- если известно, сколько таблетов таблицы в среднем на ноде и сколько доступно оперативной памяти, то `lookup_cache_rows_per_tablet` равен количеству памяти, которое можно выделить на ноду / количество таблетов на ноде / средний размер строки (Data weight таблицы / количество строк в таблице).

[*first]: Выбирает первое записанное значение, игнорирует все последующие.
