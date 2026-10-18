# HW9

## Practical, Transparent Operating System Support for Superpages

Juan Navarro, Sitaram Iyer, Peter Druschel, and Alan Cox. OSDI 2002.

These notes cover the motivation, related approaches, and Design (§§1–4), with implementation context from §5 where needed. The population-map diagrams below are explanatory examples using Alpha's actual branching factor, not reproductions of the paper's Figure 3. Evaluation (§6) is not summarized here.

## Questions and answers

**Q1. What does a TLB entry store: a VA-to-PA mapping, or a virtual-page-number-to-physical-page-number mapping?**

Conceptually, an entry caches the translation of a virtual page to a physical frame, together with attributes such as permissions and information identifying the address space and page size, as supported by the architecture. It does not store a separate translation for every byte. For a mapping of size `S`:

```text
PA = physical_base + (VA - virtual_base)
   = physical_base + offset_within_page
```

The page offset passes through unchanged. A superpage uses more offset bits and lets one entry cover more bytes. Exact TLB formats and accessed/dirty-bit mechanisms are architecture-specific; Alpha emulates reference and dirty information in software (§6.8).

**Q2. Does reserving 4 MB immediately allocate, populate, and map a 4 MB superpage?**

No. Reservation assigns a tentative contiguous physical layout. Only demanded base pages are populated and mapped. Eligible subregions can later be promoted independently; unused reserved frames can be reclaimed by preempting the reservation.

**Q3. Does changing a page mapping's size require copying the data?**

Promotion/demotion does not inherently require copying. Reservation arranges the physical layout in advance. Relocation, by contrast, explicitly copies pages to obtain a suitable layout.

## Goal and hardware constraints (§§1–2)

Increase **TLB coverage** without proportionally increasing the number of TLB entries. With a single page size, coverage is approximately `entry_count × page_size`; with multiple sizes, it depends on the sizes of the cached mappings.

Alpha in this paper supports:

| Page size | Classification | Number of 8 KB base pages |
|---|---|---:|
| 8 KB | Base page | 1 |
| 64 KB | Superpage | 8 |
| 512 KB | Superpage | 64 |
| 4 MB | Superpage | 512 |

“Superpage” is a category, not a synonym for 4 MB.

Hardware imposes these constraints:

1. The page size must be supported by the processor.
2. The virtual and physical ranges must both be contiguous.
3. Both starting addresses must be multiples of the page size.
4. All constituent pages must have compatible protection attributes.
5. A superpage exposes only coarse reference/dirty information, rather than independent information for every constituent base page.

Monotonically increasing physical addresses are insufficient: the mapping must preserve the offset throughout the region. Unconditionally using large pages can inflate memory footprints and paging I/O.

## Related approaches (§3)

| Approach | How it handles contiguity | Main tradeoff |
|---|---|---|
| Reservation | Preserve a suitable layout at allocation time | May reserve space that is never populated; needs preemption and fragmentation control |
| Relocation | Copy existing pages into a contiguous layout | Copying and mapping-maintenance costs; more robust to fragmentation |
| Hardware support | Relax or eliminate conventional contiguity constraints | Requires additional hardware |

### Reservation-based approaches (§3.1)

- **Talluri and Hill:** reserve at page-fault time; promote when population reaches a threshold; preempt reservations under pressure. Their scheme targets partial-subblock TLBs.
- **HP-UX and IRIX, as described in this paper:** eagerly populate neighboring pages and promote on a fault, even if those neighbors will not be used. The authors interpret this as a promotion threshold of one base page. Per-segment page-size hints require application/user tuning: too small loses TLB coverage, too large wastes population work and memory.

The paper's goal of **transparency** means automatically choosing and managing page sizes without requiring this tuning.

### Relocation-based approaches (§3.2)

Romer et al. use online cost-benefit analysis to decide when promotion justifies relocation. **That particular design**, not relocation in general, requires a software-managed TLB: its miss handler updates counters for potential superpages.

Besides copying costs, it waits for misses before reacting and adds work to miss handling. Reservation and relocation can also be combined, or relocation can compact memory in the background. This paper instead restores contiguity by influencing page replacement.

### Hardware-based approaches (§3.3)

- **Partial-subblock TLBs:** permit missing base-page “holes” in a larger mapping. The paper reports limited coverage gains and uncertainty about extending them to multiple superpage sizes.
- **Address remapping in the memory controller:** adds translation so nonadjacent physical pages can be organized into a superpage.

The authors report that neither special mechanism was available on commercial general-purpose machines at the time of writing. This is a historical claim, not a statement about current processors.

## Design (§4)

### Main data structures

| Structure | Responsibility |
|---|---|
| Buddy allocator | Manage available aligned physical extents; split and coalesce them |
| Reservation lists | Find partially populated reservations whose unused frames can satisfy a request |
| Population map | Track per-object population and locate associated reservations |

### Reservation-based allocation (§4.1)

On a fault requiring a physical frame:

1. Look for a reservation covering the faulting position.
2. If found, use the corresponding frame.
3. Otherwise choose a preferred size and obtain an aligned contiguous extent, potentially through reservation preemption.
4. Populate and map only the faulting base page; tentatively reserve the remaining frames.

For virtual-region base `V`, physical-reservation base `P`, and faulting base-page address `v`, use the frame at `P + (v - V)`. Access order therefore does not scramble the future superpage layout.

### Preferred superpage size (§4.2)

Favor the largest size that can effectively be used: an overlarge reservation can later be preempted, whereas enlarging an insufficient reservation may require relocation.

- **Fixed-size objects** (e.g., code and mapped files): choose the largest aligned region containing the faulting page, not overlapping existing reservations/allocations, and not extending beyond the object.
- **Growing objects** (e.g., heaps and stacks): allow the reservation to extend beyond the current object boundary, but limit its size to the object's current size. Alignment and non-overlap constraints still apply. This accommodates growth without giving tiny objects huge speculative reservations.

### Preempting reservations (§4.3)

When the requested extent is unavailable, preempt a suitable reservation if possible before settling for a smaller size. Prefer the candidate whose **most recent allocation of a new page is oldest**. Frequent reads/writes to an already populated page do not count as further population.

Preemption reclaims unused frames. It does not inherently evict or move populated pages. Their positions limit the size of the empty extents that can be recovered.

### Fragmentation control (§4.4)

Coalescing alone cannot bridge allocated pages and is insufficient under persistent memory pressure. The page daemon also favors reclaiming inactive pages that help restore needed aligned contiguous extents (§5.1). It can wake on a contiguous-allocation failure even when total available memory is substantial.

FreeBSD implementation context: clean unmapped cache pages can participate in available extents while their old contents are retained until needed. Wired, non-pageable kernel pages are clustered at allocation time to avoid scattering permanent obstacles across many large-page regions (§5.2).

### Incremental promotion (§4.5)

Promote a fully populated aligned subregion when the other constraints hold. A 4 MB reservation can contain 64 KB and 512 KB superpage mappings before the entire reservation fills.

Do not populate untouched pages merely to reach an early-promotion threshold: that would inflate the application's footprint. Population alone is necessary but insufficient; reservation/layout, permissions, and uniform dirty state also matter (§4.9).

### Speculative demotion (§4.6)

Demote when evicting a constituent base page or changing protection on part of a superpage. Recursively split only the subregion containing the target; preserve larger mappings elsewhere when possible.

An active superpage's coarse reference information cannot distinguish “every base page is hot” from “one base page is hot.” Under memory pressure, speculative demotion restores finer reference tracking so cold pages can be discovered. Referenced regions can be incrementally repromoted. Demotion changes mapping granularity; eviction is the separate act of reclaiming a populated frame.

### Paging out dirty superpages (§4.7)

**Implemented policy:** demote a clean superpage when a process attempts to write to it; repromote an eligible region after all its base pages are dirty. This avoids treating clean neighbors as dirty and writing them unnecessarily.

Demoting only at writeback time is too late: an aggregate dirty mark cannot identify which base pages were modified earlier.

**Considered but not adopted:** save a digest for each clean base page and compare it with a recomputed digest at writeback, writing only changed pages. This trades additional CPU/memory-reading work for finer writeback decisions, subject to digest collisions. Preliminary SHA-1 experiments showed up to 15% overhead on disk-intensive applications and 60% degradation in a CPU-saturated sequential-read pathological case. Delayed and idle-time hashing were proposed optimizations, not evaluated results.

### Multi-list reservation scheme (§4.8)

Classify each partially populated reservation by the **largest aligned empty extent recoverable by preempting it**, not by the reservation's own size. Alpha has 8 KB, 64 KB, and 512 KB reservation lists; no 4 MB list is needed because a live reservation already contains a populated frame.

Within each list, order records by their most recent page allocation, oldest first.

When preempting, split into smaller extents:

| Subextent | Action |
|---|---|
| Empty | Return to the buddy allocator |
| Partially populated | Keep as a smaller reservation and reinsert into the appropriate list |
| Fully populated | Retain populated pages; no unused reservation capacity remains to list |

Example request for 64 KB: try the buddy allocator, then the 64 KB reservation list, then the 512 KB list. If those fail, fall back to a base page: try the buddy allocator, then the 8 KB list.

## Population map in detail (§4.9)

### 1. What the tree represents

A population map is a **radix tree over a memory object's page positions**, recording which base pages have been populated. Each level corresponds to a supported size:

```text
4 MB root
  -> 8 children of 512 KB each
       -> 8 children of 64 KB each
            -> 8 leaves of 8 KB each
```

Each root covers 512 base-page positions. A large object can have multiple maps. A hash table locates a map using `(memory object, starting page index within that object)`; the starting index identifies the maximum-page-sized region.

For a base-page index `n` within one 4 MB region, the path is:

```text
512 KB child index = n // 64
 64 KB child index = (n // 8) % 8
  8 KB leaf index  = n % 8
```

`//` means integer division. Example: `n = 75` selects child `1`, then `1`, then leaf `3`. The nine index bits are split into three groups of three. The 13-bit byte offset within an 8 KB page is separate.

The population map is queried during OS memory management. It is not the page table used to translate every normal memory access, and it does not record page hotness or dirty state in its two population counters.

### 2. Read the counters correctly

For an internal node with `R = 8` direct children:

```text
somepop = number of direct children containing at least one populated base page
fullpop = number of direct children whose entire region is populated
```

Both count **direct child regions**, not all populated base pages in the subtree.

![Population map tree with actual Alpha sizes and eight-way branching](assets/population-tree.png)

**Figure A — Bottom-up reading.** In the illustrated 512 KB region, three 64 KB children are nonempty and only one is full, so its counters are `(3, 1)`. Its parent has one nonempty 512 KB child and none full, so the 4 MB root is `(1, 0)`. Omitted empty groups represent the remaining children; they are not extra levels.

Useful invariants:

| Counter condition | Meaning |
|---|---|
| `somepop == 0` | Entire region is unpopulated |
| `somepop == R` | Every child is nonempty; the region need not be full |
| `fullpop == R` | Entire region is populated |
| `0 <= fullpop <= somepop <= R` | Always holds |

At the 64 KB level, each direct child is one base page, either empty or populated; consequently `somepop == fullpop` there. Above that level, a child can be partially populated, so the two counts can differ.

**Do not sum child counters to get the parent's counters.** The parent asks only whether each child is nonempty and whether it is full.

The paper's original Figure 3 uses four-way branching for illustration. Its `(4, 4)` means all four child regions are full, not necessarily four base pages. The actual Alpha implementation uses `R = 8`, as in Figure A above.

### 3. Lazy construction and counter propagation

Create tree nodes as needed; an absent child is equivalent to zero population counters. This absence says nothing by itself about a physical reservation inherited from an ancestor.

For population/insertion, notify a parent only when a child's classification changes:

- Empty -> nonempty: increment the parent's `somepop`.
- Not full -> full: increment the parent's `fullpop`.
- Still nonempty and not full: neither parent counter changes.

![How population counters change as base pages are allocated](assets/population-updates.png)

**Figure B — Sequential population of the first 512 KB.** The first base page makes its 64 KB, 512 KB, and 4 MB regions nonempty. The second base page changes only the lowest internal node. The eighth fills the first 64 KB child. The sixty-fourth fills the first 512 KB child, so the root's `fullpop` becomes one. Each tuple is `(somepop, fullpop)`.

Thus, allocating a base page does not imply changing counters at every ancestor. Structural allocation/traversal and counter propagation are distinct operations.

### 4. Population is separate from reservation

A node that has a reservation points to its reservation record; that record points back to the node:

```text
population-map node <----> reservation record ----> physical extent
        |
        +---- child nodes describe population inside this region
```

The reservation record also participates in the appropriate reservation list. These are different views of the same reservation: the tree locates it by address, while the list selects it for preemption.

If a 4 MB node has a reservation, even an unpopulated descendant may already have a corresponding reserved physical frame. It does not require a separate reservation record at every leaf.

```text
Virtual region base: V          Physical reservation base: P
Target base page: V + k*8 KB -> Corresponding frame: P + k*8 KB
```

An empty population count means “no populated data here,” not “no reservation here.” Conversely, a full count alone does not establish that separately allocated frames form a valid superpage layout.

### 5. Four operations supported by the tree

**Reserved-frame lookup.** Identify the maximum-page-sized region containing the fault, locate its map through the hash table, and traverse the address path for a covering reservation. Use the region-relative offset to find the physical frame.

**Overlap avoidance.** If no reserved frame covers the fault, walk from the root toward the leaf. The first node with `somepop == 0` identifies the largest empty region on that path. This bounds a new reservation; the preferred-size policy's object-boundary and other constraints still apply. Existing reservation lookup must precede treating an unpopulated position as available for a new reservation.

**Promotion decisions.** After servicing the fault, walk from the root toward the faulting position and try the first fully populated node that has an associated reservation. Root-first traversal finds the largest such candidate. Promotion also requires uniform protection attributes and dirty state in the faulting process's mappings. Occupancy counters alone do not prove promotion eligibility.

**Preemption assistance.** Follow the reservation's back pointer to its node. Child counts identify empty extents to return and partially populated extents to retain/relist. Already populated pages remain in place.

### 6. Checks against common misunderstandings

| Claim | Correction |
|---|---|
| A 4 MB root means a 4 MB page mapping exists | It only identifies a 4 MB management region |
| `somepop = 3` means three base pages are populated | It means three direct child regions are nonempty |
| The parent sums children's counters | It counts children satisfying nonempty/full predicates |
| A missing child means its physical frame is unreserved | A reservation may be attached to an ancestor |
| Fully populated means promotable | Layout/reservation, permissions, and dirty-state constraints still apply |
| The tree is a second page table | It tracks object population/reservations for management, not ordinary access translation |

**Self-check:** A 512 KB node is `(8, 0)`. How many 8 KB pages are populated?

**Answer:** The exact number is unknown. Each of its eight 64 KB children contains between one and seven populated base pages, so the total lies between **8 and 56**. This illustrates precisely what the counters retain and what they summarize away.

## End-to-end allocation path

```text
Fault needing a physical frame
  -> locate population map
  -> covering reservation exists?
       yes: use its corresponding frame
       no: choose size -> obtain extent/preempt reservation -> create reservation
  -> populate and map the demanded base page
  -> update population counters and reservation-list bookkeeping
  -> attempt the largest eligible promotion
```

Source: the supplied transcription of the paper, especially §§2–4.9; implementation details are identified above by section. Diagrams A and B and numerical examples were constructed for these notes.

## Questions: The paper's Section 4.5 says that a superpage mapping is only inserted in a process's page table after the process has referenced every base (ordinary) page in the superpage's address range. Section 4.1 says that the physical RAM for an entire superpage is reserved on the very first reference to any base page in the superpage's address range. Given the latter policy, why doesn't the design also insert the superpage mapping at the same time, when the process first references any page in the superpage's address range?

Reserving physical frames does not mean populating and mapping them. The design initially populates and maps only the referenced base page, while unused reserved frames remain reclaimable through reservation preemption. Inserting a superpage mapping immediately would require preparing valid contents for the entire range, such as zero-filling anonymous pages or reading file-backed pages. This would increase memory consumption and population costs even if the process never accesses most of the range. Therefore, the design waits until the range is fully populated before promoting it.
