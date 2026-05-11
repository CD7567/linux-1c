# Демо работы

## 1. Выделение heap памяти через malloc

```c
char *heap = malloc((size_t)page_sz * 2);
if (!heap) {
    perror("malloc");
    return 1;
}

strcpy(heap, "hello from heap");
```

Выделяем себе память через `malloc` и пишем в нее, чтобы материализовать страницу. 

### 1.1 syscall через выровненный указатель
```c
run_case("heap: malloc base", heap);
```

Ожидаем увидеть `PRESENT` страницу с дополнительными флагами `ANON` (так как это heap память) и `SOFT-DIRTY` (поскольку произвели запись).


```
== heap: malloc base ==
input VA : 0x0000559ccdbdc6b0
status   : 0 (PRESENT)
pfn      : 0x000000000012709b
pa       : 0x000000012709b6b0
flags    : 0x0000000000000025 [PRESENT|ANON|SOFT_DIRTY]
```

### 1.2 syscall через невыровненный указатель


```c
// Code from case 1
run_case("heap: malloc base", heap + 17);
```

Ожидаем такую же общую картину, что и в пункте 1, причем `pfn` будет тем же, так как мы находимся на той же странице памяти.

```
== heap: malloc + 17 ==
input VA : 0x0000559ccdbdc6c1
status   : 0 (PRESENT)
pfn      : 0x000000000012709b
pa       : 0x000000012709b6c1
flags    : 0x0000000000000025 [PRESENT|ANON|SOFT_DIRTY]
```

## 2. mmap

```c
char *anon = mmap(NULL, (size_t)page_sz * 3,
            PROT_READ | PROT_WRITE,
            MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);

anon[0] = 'X';
anon[page_sz] = 'Y';
anon[2 * page_sz] = 'Z';
```

Пытаемся аллоцировать память через `mmap` на размер 3 страниц и пишем в каждую из них.

### 2.1 Доступ к нулевой странице
```c
run_case("anon mmap: page 0", anon);
```

Ожидаем увидеть `PRESENT` страницу с дополнительными флагами `ANON` (передали такой флаг в mmap) и `SOFT-DIRTY` (поскольку произвели запись).

```
== anon mmap: page 0 ==
input VA : 0x00007f1d1fc85000
status   : 0 (PRESENT)
pfn      : 0x00000000001218c5
pa       : 0x00000001218c5000
flags    : 0x0000000000000025 [PRESENT|ANON|SOFT_DIRTY]
```

### 2.2 Доступ к первой странице
```c
run_case("anon mmap: page 1", anon + page_sz);
```

Ожидаем такую же общую картину, что и в пункте 1, причем `pfn` уже отличается, так как получаем доступ к новой странице (причем оффсет будет таким же `0x000`).

```
== anon mmap: page 1 ==
input VA : 0x00007f1d1fc86000
status   : 0 (PRESENT)
pfn      : 0x00000000001218cc
pa       : 0x00000001218cc000
flags    : 0x0000000000000025 [PRESENT|ANON|SOFT_DIRTY]
```

### 2.3 Доступ к третьей странице
```c
run_case("anon mmap: page 2 + 123", anon + 2 * page_sz + 123);
```

Ожидаем такую же общую картину, что и в пунктах 1 и 2, `pfn` также будет другим из-за новой страницы, но еще и изменится оффсет, поскольку мы сами сделали невыровненный указатель.

```
== anon mmap: page 2 + 123 ==
input VA : 0x00007f1d1fc8707b
status   : 0 (PRESENT)
pfn      : 0x0000000000121b7a
pa       : 0x0000000121b7a07b
flags    : 0x0000000000000025 [PRESENT|ANON|SOFT_DIRTY]
```

## 3. mmap с файлом
```c
int fd = create_backing_file(tmp_path, sizeof(tmp_path), (size_t)page_sz * 2);

char *filemap = mmap(NULL, (size_t)page_sz * 2,
                     PROT_READ | PROT_WRITE,
                     MAP_PRIVATE, fd, 0);

volatile char c = filemap[0];
```

Создаем временный файл и выделяем с его помощью себе память.

### 3.1 Доступ к нулевой странице
```c
run_case("file-backed mmap: page 0", filemap);
```

Ожидаем увидеть `PRESENT` страницу с дополнительным флагом `FILE` (так как мы сами его установили), но без `SOFT-DIRTY` (так как произвели только чтение).

```
== file-backed mmap: page 0 ==
input VA : 0x00007f1d1fa8b000
status   : 0 (PRESENT)
pfn      : 0x0000000000121b75
pa       : 0x0000000121b75000
flags    : 0x0000000000000009 [PRESENT|FILE]
```

### 3.2 Доступ к первой странице
```c
run_case("file-backed mmap: page 1 + 64", filemap + page_sz + 64);
```

Ожидаем такую же общую картину, что и в пункте 1, причем `pfn` уже отличается, так как получаем доступ к новой странице, также отличается и оффсет, так как сами сделали невыровненный указатель.

```
== file-backed mmap: page 1 + 64 ==
input VA : 0x00007f1d1fa8c040
status   : 0 (PRESENT)
pfn      : 0x000000000012709c
pa       : 0x000000012709c040
flags    : 0x0000000000000009 [PRESENT|FILE]
```

## 4. Доступ к UNMAPPED памяти
```c
char *anon_saved = anon;
if (munmap(anon, (size_t)page_sz * 3) != 0) {
    perror("munmap anonymous");
} else {
   run_case("after munmap: expected UNMAPPED", anon_saved);
}
```

Берем память из примера выше и делаем `munmap`, ожидаем получить `UNMAPPED` результат.

```
== after munmap: expected UNMAPPED ==
input VA : 0x00007f1d1fc85000
status   : 2 (UNMAPPED)
pfn      : 0x0000000000000000
pa       : 0x0000000000000000
flags    : 0x0000000000000000 [none]
```

## 5. Kernel-like большой адрес
```c
run_case("kernel-like high address: expected NOACCESS",
             (void *)(uintptr_t)0xffff800000000000ULL);
```

Передаем очень большой адрес, который должен триггернуть проверку на границу доступа к данным ядра. Ожидаем поймать `NOACCESS`.

```
== kernel-like high address: expected NOACCESS ==
input VA : 0xffff800000000000
status   : 3 (NOACCESS)
pfn      : 0x0000000000000000
pa       : 0x0000000000000000
flags    : 0x0000000000000000 [none]
```

## 6. Передаем NULL в syscall
```c
run_case("NULL address", NULL);
```

Обычно VMA процесса должны начинаться выше, поэтому мы точно не ожидаем разумных данных. Здесь чисто проверяем, что не упадем на граничных значениях, а получим `UNMAPPED`.

```
== NULL address ==
input VA : 0x0000000000000000
status   : 2 (UNMAPPED)
pfn      : 0x0000000000000000
pa       : 0x0000000000000000
flags    : 0x0000000000000000 [none]
```

## 7. Берем на heap, но не аллоцированную нами
```c
run_case("heap far offset (best-effort experiment)", heap + page_sz * 16);
```

Пробуем забраться повыше реально аллоцированной нами памяти (даже с учетом возможного выделения впрок большего блока, чем мы попросили).

```
== heap far offset (best-effort experiment) ==
input VA : 0x0000559ccdbec6b0
status   : 2 (UNMAPPED)
pfn      : 0x0000000000000000
pa       : 0x0000000000000000
flags    : 0x0000000000000004 [ANON]
```

Ожидаемо получили `UNMAPPED`, так как это не выделенная память, но при этом получили `ANON` флаг. Этот виртуальный адрес лежит внутри анонимного VMA, но конкретная страница по нему не описана в таблицах страниц (нет PTE или она не present).
