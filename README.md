# fsearch — a local file search engine in C++

`fsearch` builds an inverted index over a directory (filenames + text file
contents) and answers keyword queries far faster than scanning files on
every search. It's a small, from-scratch version of the core idea behind
most real search systems: **don't re-read the data on every query — index
it once, then query the index.**

## Why this exists

OS-level file search is often slow or limited to filenames. This project
scopes the same problem down to something a single engineer can build well:
crawl a directory, tokenize content, build an inverted index, and serve
ranked queries against it — with a persisted index so you don't have to
re-crawl every time.

## Architecture

```
crawler.cpp        walks the filesystem (std::filesystem), reads text-like
                    files, always captures filenames (so binaries/images
                    are still findable by name)

tokenizer.cpp       lowercases + splits text into alphanumeric tokens

inverted_index.cpp  term -> [(doc_id, term_frequency), ...]
                    - build: O(total tokens)
                    - query: intersects posting lists, smallest list first
                    - binary save/load for persistence

main.cpp            CLI: `fsearch build <dir> <index>`
                          `fsearch search <index> <query...>`
```

### Indexing

Each document gets a `doc_id`. For every unique term in the document, we
append a `(doc_id, term_frequency)` posting to that term's list. Build time
is linear in the total number of tokens across all files: **O(N)**.

### Querying

Query terms are AND-combined (a result must contain every term). We fetch
each term's posting list, sort by list length ascending, and intersect
starting from the smallest list — a standard inverted-index optimization
that keeps the hot path small when one term is much rarer than the others.
Score is the sum of term frequencies across matched terms; results are
sorted descending and truncated to `topK`.

### Persistence

The index is serialized to a flat binary file (doc paths, then term →
posting-list pairs with raw byte writes). No compression — see Roadmap.

## Build & run

```bash
make
./fsearch build /path/to/directory my_index.idx
./fsearch search my_index.idx invoice report
```

## Benchmark: indexed search vs. naive `grep -r`

`benchmark/bench.sh` builds an index once, then times N repeated queries
against `fsearch search` vs. `grep -r` cold-scanning the same directory
every time.

On a synthetic set of **3,000 files** (50 random words each), 30 repeated
single-term queries:

| Method              | Total (s) | Avg / query (ms) |
|----------------------|-----------|-------------------|
| `fsearch` (indexed)  | 0.106     | 3.52              |
| `grep -r` (naive)    | 0.309     | 10.30             |

~3x faster **including process-startup and index-load overhead on every
call**. A persistent server process that keeps the index resident in
memory (instead of loading it fresh each CLI invocation) would remove that
overhead and widen the gap — this is exactly the daemon-vs-CLI tradeoff
real search infra deals with (noted below as a next step).

Run it yourself:

```bash
./benchmark/bench.sh /path/to/large/directory sometoken 30
```

The gap widens with corpus size and query volume, since `grep -r` always
re-scans every file while `fsearch`'s query cost depends only on the
matching posting lists.

## Design decisions & tradeoffs

- **Strict AND semantics, not OR / ranked-any-match.** Simpler to reason
  about and matches most people's mental model of multi-word search.
  Documented as the first thing to revisit if this became a real product.
- **Term-frequency scoring, not TF-IDF.** TF-IDF would down-weight common
  words automatically; skipped for v1 to keep the index build a single
  pass with no second pass for document-frequency stats. Straightforward
  to add (see Roadmap).
- **Extension allow-list for content indexing.** Keeps the crawler from
  trying to tokenize binaries. Filenames are indexed regardless, so
  non-text files remain searchable by name.
- **No stemming/stopword removal.** Deliberately left out to keep the
  tokenizer trivial to audit; noted as the most obvious near-term quality
  improvement.

## Roadmap / stretch goals

- **Incremental updates via filesystem watching** (`inotify` on Linux) —
  update the index as files change instead of re-crawling.
- **TF-IDF or BM25 ranking** instead of raw term frequency.
- **Fuzzy / prefix matching** via a trie or edit-distance tolerance, for
  typo-resistant search.
- **Compressed postings** (delta encoding + varints) — the current binary
  format is intentionally naive.
- **Long-running daemon + socket protocol** instead of a CLI that reloads
  the index from disk each invocation — removes the load-time overhead
  visible in the benchmark above and is the natural next step toward a
  "real" search service.
- **Distributed indexing** — sharding the index across multiple machines
  is the natural next question this project sets up ("how would this work
  if the corpus didn't fit on one disk?").

## What this demonstrates

Inverted-index construction and query-time intersection, filesystem
traversal via `std::filesystem`, binary serialization, and basic
algorithmic tradeoffs (posting-list-size-first intersection). The roadmap
above is intentionally the same shape as a systems-design interview
follow-up: given a working single-machine version, how would you scale it?
