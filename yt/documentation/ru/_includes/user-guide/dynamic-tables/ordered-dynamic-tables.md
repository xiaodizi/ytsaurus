# Упорядоченные динамические таблицы

В данном разделе рассмотрено устройство упорядоченных динамических таблиц, описаны действия, которые можно выполнять с данным видом таблиц.

## Модель данных { #model }

Упорядоченные динамические таблицы — это разновидность таблиц в {{product-name}}, представляющая собой простую упорядоченную последовательность строк. Каждая строка упорядоченной таблицы состоит из колонок и подчинена схеме таблицы, указываемой при создании. Ключевые колонки в таких таблицах отсутствуют. 

Упорядоченные динамические таблицы поддерживают добавление новых строк в конец в рамках транзакции, а также чтение строк по их индексам без транзакционной изоляции. 

Ближайшим аналогом упорядоченных динамических таблиц является [Apache Kafka](https://kafka.apache.org).

Как и в случае [сортированных динамических таблиц](../../../user-guide/dynamic-tables/sorted-dynamic-tables.md), пространство ключей упорядоченной динамической таблицы делится на [таблеты](../../../user-guide/dynamic-tables/overview.md#tablets). 

Разделение данных на таблеты произвольно. Каждый таблет содержит упорядоченную последовательность строк таблицы. При записи строк в таблет они попадают в конец этой последовательности. Тем самым, упорядоченность гарантируется лишь в пределах таблета.

 Упорядоченная динамическая таблица исходно состоит из одного таблета. Множество таблетов можно менять с помощью команды `reshard_table`. Она позволяет изменить структуру для набора таблетов, идущих подряд. В упорядоченных динамических таблицах при перешардировании указывается новое количество таблетов, на которые следует заменить исходные. Указание `pivot_keys` не требуется. Существующие данные перераспределяются между новыми таблетами неспецифицированным образом.

## Поддерживаемые операции { #methods }

### Создание

Для создания динамической упорядоченной таблицы необходимо выполнить команду `create table`, указав в атрибутах схему и настройку `dynamic=True`. Схема должна соответствовать схеме упорядоченной таблицы, в частности, среди её колонок не должно быть ключевых. 

CLI
```bash
yt create table //path/to/table --attributes \
'{dynamic=%true;schema=[{name=first_name;type=string};{name=last_name;type=string}]}' 
```

### Запись строк

Для записи в упорядоченную динамическую таблицу служит команда `insert_rows`. По умолчанию данные записываются в случайный замонтированный таблет. Для того, чтобы управлять распределением данных в ручном режиме, можно в записываемых строках использовать специальную системную колонку `$tablet_index`, имеющую тип `int64`. Значения в этой колонке должны представлять собой числа от 0 до N - 1, где N — число таблетов в таблице. Соответствующие строки будут записаны строго в указанный таблет. Строки, для которых такая аннотация отсутствует, будут записаны в случайный смонтированный таблет.

Запись, произведенная в рамках транзакции, транзакционна: если транзакция завершается успешно, то строки появляются в соответствующих таблицах, если неудачно, то не появляются. При этом в рамках одной транзакции можно работать как с сортированными, так и с упорядоченными таблицами.

### Чтение строк

Читать данные из упорядоченных таблиц можно с помощью SQL-подобного языка запросов и команды `select_rows`. При этом каждая упорядоченная динамическая таблица предстаёт как сортированная с системными ключевыми колонками `($tablet_index, $row_index)` (оба типа `int64`), а также всеми указанными в схеме таблицы колонками данных.

Например, так выглядит запрос, читающий диапазон строк из фиксированного таблета упорядоченной таблицы:

```sql
* from [//path/to/table] where [$tablet_index] = 10 and [$row_index] between 100 and 200 
```

### Изменение схемы и типа таблицы

Изменить схему уже существующей динамической таблицы можно с помощью команды `alter-table`. Для успешного выполнения команды таблица должна быть [отмонтирована](../../../user-guide/dynamic-tables/overview.md#mount_table), а новая схема должна быть совместима со старой. При этом никаких изменений в записанных на диск данных не происходит, поскольку старые данные удовлетворяют новой схеме. 

С помощью `alter-table` можно превратить динамическую таблицу в статическую. Подробности можно узнать в разделе [MapReduce по динамическим таблицам](../../../user-guide/dynamic-tables/mapreduce.md#convert_table).

### Trim

{% note warning "Внимание" %}

Использование `reshard` вместе с `trim` запрещено, так как может приводить к непредвиденным последствиям.

{% endnote %}

В общем случае из упорядоченной динамической таблицы нельзя удалить данные. Но существует исключение: в каждом таблете можно удалить начальный отрезок строк. Для этого необходимо использовать команду `trim_rows`. В качестве аргументов ей передаётся путь к таблице, номер таблета, а также аргумент `trimmed_row_count`, показывающий сколько строк в таблице будет удалено после выполнения данной команды. При таком удалении нумерация строк сохраняется. Например, при первом вызове для `trimmed_row_count = 10` будут удалены строки с номерами с 0 по 9 включительно. Затем при вызове с `trimmed_row_count = 30` — строки с 10 по 29 включительно и т. д. `trimmed_row_count` имеет не относительный смысл, а абсолютный и указывает не количество строк, которые будут дополнительно удалены при очередном вызове, а какие именно начальные строки будут удалены после вызова.

Команда `trim_rows` выполняется вне транзакций. После того как она отработала, удаленные данные уже нельзя прочитать командой `select_rows`. Как только оказывается, что в таблете удалено столько строк, что они образуют целый начальный чанк, узел кластера, обслуживающий таблет, посылает сигнал мастер-серверу, и данный чанк удаляется целиком. Именно в этот момент освобождается дисковое пространство.

Количество удаленных строк в любом таблете можно узнать из атрибута `trimmed_row_count` таблета. Данный параметр обновляется асинхронно, то есть между выполнением `trim_rows` и его изменением может пройти некоторое время.

При отмонтировании и последующем монтировании таблета число начальных удаленных строк сохраняется, что гарантирует неизменность нумерации.

{% note warning %}

При превращении динамической таблицы в статическую с помощью команды `alter_table` информация о том, какие строки были удалены, теряется. Также теряется и порядок следования строк в таблице. Порядок строк внутри чанков остается неизменным, но на это сложно опираться. Действительно, у динамической таблицы удаленными могут быть помечены некоторые начальные строки некоторых чанков, и эта информация не может быть сохранена при превращении таблицы в статическую. В итоге получается, что при конвертации динамической таблицы в статическую возможно появление в таблице некоторых из ранее удаленных строк.

{% endnote %}

### Решардирование

Если при шардировании число таблетов увеличивается, то существующие таблеты не меняются, а новые таблеты создаются пустыми. Если же при решардировании требуется уменьшить число таблетов, таблеты в конце диапазона склеиваются с последним из результирующего списка таблетов, который не входит в диапазон.

При этом система старается сохранить инвариант "то, что удалено, более не становится доступно". Поскольку при решардировании система добавляет чанки из удаляемых таблетов в конец последнего сохраняющегося, то при наличии удаленных строк в удаляемых таблетах решардирование затруднено. Для успешного завершения решардирования требуется, чтобы в удаляемых таблетах удаленные строки образовывали префикс по чанкам.

Практически данное ограничение сложно соблюдать, поэтому можно считать, что решардирование с уменьшением числа таблетов невозможно, если в удаляемых таблетах есть удаленные строки. Чтобы обойти указанное ограничение, можно воспользоваться двумя вызовами `alter_table`, превратив таблицу сначала в статическую, а потом в динамическую. Но важно помнить, что при таком способе в таблице вновь станут видны некоторые из удаленных строк.

### Автоматическое удаление старых строк (TTL)

К данным упорядоченных таблиц применяются те же настройки удаления старых данных, что и у сортированных таблиц. Подробнее можно прочитать в разделе [Удаление старых данных](../../../user-guide/dynamic-tables/sorted-dynamic-tables.md#remove_old_data). Существенное отличие состоит в том, что в упорядоченных таблицах у строки всегда есть только одна версия. Тогда настройки по очистке можно интерпретировать следующим образом:

- если `min_data_versions > 0` (по умолчанию значение 1), то автоматического удаления не происходит;
- система {{product-name}} не удаляет строки, записанные менее чем за `min_data_ttl` до текущего момента;
- если `max_data_versions = 0`, можно удалять строки, записанные позднее `min_data_ttl`;
- если `max_data_versions > 0`, можно удалять строки, записанные позднее `max_data_ttl`.

{% note info "Примечание" %}

Автоматическое удаление (`trim`) применяется сразу ко всему чанку. Пока в чанке есть строки, которые нельзя удалить, все строки из чанка будут доступны.

{% endnote %}

## Колонка $timestamp

В схеме упорядоченной динамической таблицы можно указать специальную системную колонку `$timestamp` типа `uint64`. Значение в данной колонке формируется системой автоматически при записи, оно равно `commit timestamp` для транзакции, в которой строки оказались добавлены в таблицу.

## Колонка $cumulative_data_weight

В схеме упорядоченной динамической таблицы можно указать специальную системную колонку `$cumulative_data_weight` типа `int64`. Значение в данной колонке формируется автоматически при записи. Оно равно суммарному логическому весу строк в байтах в таблете, считая от начальной строки с индексом ноль до текущей, включительно. Вес самой колонки `$cumulative_data_weight` также учитывается в этом значении. 

При добавлении этой колонки в схему уже существующий таблицы (через отмонтирование и запрос `alter_table`), начальное значение `$cumulative_data_weight` берется из метаданных чанков таблицы.

## Видимость изменений, strong/weak commit ordering

Уровень консистентности упорядоченных динамических таблиц принципиально ниже такового для сортированных: закоммиченные данные в общем случае могут быть видны не сразу после коммита, а также могут добавиться не в том порядке, в котором коммиты происходили.

При использовании распределенного коммита — когда его участниками оказываются несколько таблет-селлов — оказывается, что непосредственное появление данных в таблетах различных таблет-селлов происходит в различные моменты времени. Действительно, вторая фаза протокола (коммит) выполняется физически распределенными участниками. Более того: один и тот же таблет-селл может быть участником нескольких распределенных транзакций, причем значения их `commit-ts` и фактическая последовательность, в которой данный таблет-селл выполняет коммит, в общем случае не подчинены никаким условиям. Участник может выполнить коммит транзакции `A` до коммита транзакции `B` даже когда `commit-ts(A) > commit-ts(B)`.

В случае динамических сортированных таблиц указанная деталь реалиации скрыта от пользователя snapshot-изоляцией: несмотря на то, что данные из траназкции `A` уже добавлены в таблицу, прочитать их можно лишь заказав `ts`, не меньший `commit-ts(A)`. Кроме того, система блокировок строк гарантирует, что подобная инверсия не будет наблюдаться на одном и том же ключе в случае, когда интервалы `[start-ts,commit-ts]` для транзакций пересекаются.

Для динамических упорядоченных таблиц ситуация иная: обычно они не хранят commit timestamps, исключая случай поля `$timestamp`, и не поддерживают snapshot-изоляцию. Поэтому порядок, в котором записи будут добавляться в конец упорядоченной динамической таблицы не имеет ничего общего с порядком на commit timestamps.

В случае, когда упорядоченная динамическая таблица участвует в двухфазном коммите, то есть коммит затрагивает более одного таблет-селла, то подтверждение успешного коммита от координатора не значит, что все участники также произвели коммит и данные действительно добавлены в конец таблицы. Например, это верно в случае всех коммитов, в которых упорядоченная динамическая таблица является синхронной репликой. В указанном сценарии двухфазного коммита сразу после успешного его окончания попытка найти закоммиченные строки на участнике может закончиться неудачей: эти строки станут видны лишь через некоторое время.

Можно получить определенные гарантии монотонности для добавленных строк. У таблицы есть системный атрибут `commit_ordering`, который управляет порядком занесения строк в таблицу:

- `weak` — режим по умолчанию, строки попадают в упорядоченную таблицу сразу в момент коммита участника, который запаздывает относительно координатора, порядок относительно commit timestamps не гарантируется;
- `strong` — гарантируется, что строки попадают в таблицу в порядке commit timestamps.

В режиме `strong` при наличии поля `$timestamp` таблица оказывается упорядоченной по этому полю, но это не делает его ключевым. 

Фактически режим `strong` реализован следующим образом: каждый таблет-селл отслеживает специальный barrier-ts, который постоянно монотонно возрастает и про который известно, что ни одна транзакция не может получить `commit-ts` меньше `barrier-ts`. Строки, записанные в рамках транзакции в упорядоченную динамическую таблицу с режимом `strong`, попадают в нее не сразу в момент коммита, когда barrier-ts становится больше `commit-ts` данной транзакции. Тем самым, система сериализует все транзакции по `commit-ts`, но только те, для которых `commit-ts < barrier-ts`. Для транзакций с `commit-ts > barrier-ts` система может определить относительный порядок, но не может гарантировать, что в будущем не появится новой транзакции, которая нарушит установленный порядок. 

Для статических таблиц атрибут `commit_ordering` также присутствует, но он всегда равен `weak`.