# PES-VCS: A Lightweight Version Control System

**Student Name:** Dabbugunta Venya Anand 
**SRN:** PES1UG24AM074 


---

## 1. Project Overview
This project is a functional, Git-inspired Version Control System (VCS) implemented in C. It features a sharded object store for content-addressed storage, a staging area for managing changes, and a commit system to track repository history through linked objects.

---

## 2. Phase Screenshots

### Phase 1: Object Store
* **1A: Test Objects Output**
  <img width="786" height="137" alt="image" src="https://github.com/user-attachments/assets/fecec131-23bf-4995-9832-c4abcd66a25c" />

  
    *Output showing passing tests for blob storage, deduplication, and integrity.* 

* **1B: Sharded Directory Structure**

<img width="795" height="117" alt="image" src="https://github.com/user-attachments/assets/86e0173f-144d-49a5-97af-faafe94600cf" />
    
    Verification of the .pes/objects directory structure showing two-character sharding. 

### Phase 2: Tree Construction
* **2A: Test Tree Output**

<img width="687" height="200" alt="image" src="https://github.com/user-attachments/assets/5e4a7e1f-4830-4419-96ce-e6b89320dfd2" />

    *Confirmation of successful tree serialization and deterministic hashing.* 

* **2B: Raw Tree Hex Dump**

  <img width="844" height="388" alt="image" src="https://github.com/user-attachments/assets/6595db76-14e3-4f1e-bf3b-092bd2c3d396" />

  
    *Binary representation of a tree object showing file modes, hashes, and names.* 

### Phase 3: Staging Area (Index)
* **3A: Add and Status Sequence**

<img width="794" height="195" alt="image" src="https://github.com/user-attachments/assets/553701be-9745-47ec-8e3f-d7decac08f36" />

    *Workflow demonstrating repository initialization and staging files for commit.*

* **3B: Index File Content**

  <img width="874" height="114" alt="image" src="https://github.com/user-attachments/assets/722061cc-cf35-46af-b267-ca16dd713295" />

    *The internal state of the .pes/index file in text format.* 

### Phase 4: Commits and History
* **4A: Commit Log**

 <img width="803" height="101" alt="image" src="https://github.com/user-attachments/assets/ca066024-2ad9-44df-8e2f-55b0b9aee4d3" />


    *The output of `./pes log` showing the commit history, authors, and timestamps.*

* **4B: Object Growth**

<img width="798" height="179" alt="image" src="https://github.com/user-attachments/assets/4d39e77c-e18d-4c97-9ac5-92d6ee7fb684" />

    *A list of all objects generated in the .pes directory after several operations.* 

* **4C: Branch References**

<img width="650" height="200" alt="image" src="https://github.com/user-attachments/assets/3d095af3-73fb-4386-8538-93efd3fe1edd" />
 
    *The current hash stored in the main branch reference file.* 



### FINAL - make test-integration

<img width="710" height="560" alt="image" src="https://github.com/user-attachments/assets/67be23fa-0b41-44e3-89e0-493d70c8ea18" />

<img width="675" height="487" alt="image" src="https://github.com/user-attachments/assets/d9246d11-7ba8-4dce-a7c2-a8de46e9e23b" />




---

## 3. Analysis Questions

### Section 5: Branching and Checkout

### Phase 5 — Analysis: Branching and Checkout

**Q5.1 — How would you implement `pes checkout <branch>`?**

**Ref Update and Workspace Sync:**
1. **Reference Redirection:** The `.pes/HEAD` file must be rewritten to point to the target branch reference (e.g., changing `ref: refs/heads/main` to `ref: refs/heads/feature-x`).
2. **Filesystem Alignment:** The system must read the root tree of the destination commit. It then performs a recursive traversal: files present in the destination but not in the current workspace are created; files that differ are overwritten; and files present in the current workspace but missing in the destination tree are unlinked (deleted).

**Complexity Factors:**
The primary difficulty lies in **state atomicity**. A checkout is a destructive operation. If the process is interrupted (e.g., a crash or power loss), the working directory could be left in a "half-changed" state. Furthermore, the system must perform a three-way comparison between the current HEAD, the Index, and the target Tree to ensure no uncommitted user data is accidentally clobbered.

**Q5.2 — Detecting "Dirty Working Directory" Conflicts**

To prevent data loss, the system identifies conflicts using a multi-step check:
1. **Stat Comparison:** First, it compares the physical file's metadata (`mtime`, `size`) against the entries in the Index. If they don't match, the file is potentially "dirty."
2. **Content Verification:** For metadata mismatches, a SHA-256 hash of the working file is generated. If this hash differs from the Index hash, the file is officially dirty.
3. **Collision Logic:** If a file is dirty AND its content in the target branch's tree is different from the version in the current Index, the checkout is aborted. If the dirty file is identical to the version in the target branch, the checkout can safely proceed.

**Q5.3 — Detached HEAD State and Recovery**

A "Detached HEAD" occurs when the `.pes/HEAD` file contains a raw 64-character hex hash instead of a symbolic reference to a branch. In this state:
- Commits are still possible and will point to the previous hash as their parent.
- However, since no branch pointer (like `main`) "follows" these commits, they exist in isolation.

**Recovery:** If a user accidentally switches away, the commits become "orphaned." They can be rescued by creating a new branch pointer at that specific hash before the garbage collector prunes them:
## Phase 6 — Garbage Collection

### Q6.1 — Algorithm to find and delete unreachable objects

**Mark-and-sweep:**

*Mark phase — build the reachable set:*

1. Enumerate every file under `.pes/refs/` and read `HEAD` to get the GC roots.
2. For each root commit hash, call `object_read`. Add its hash, the tree hash,
   and every blob/subtree hash found by recursively walking the tree to a
   `reachable` set. Follow each commit's parent pointer until `has_parent == 0`.

*Data structure:* a sorted array of `ObjectID` (32 bytes each) with binary
search — O(n log n) to build, O(log n) per lookup. A hash table gives O(1)
average lookup for larger repos.

*Sweep phase — delete unreachable objects:*

Walk every file under `.pes/objects/XX/`. Reconstruct the full hash from the
directory name and filename, convert with `hex_to_hash`. If not in `reachable`,
call `unlink()`.

**Estimate for 100,000 commits, 50 branches:**

Assuming roughly four unique objects per commit on average (one commit, one to
two trees, one to two new blobs, the rest shared):

- Reachable set ≈ 400,000 objects → 400,000 `object_read` calls in the mark
  phase.
- Sweep: walk all ~400,000 files in the object store.
- Total: roughly **800,000 file accesses**.

### Q6.2 — Race condition between GC and a concurrent commit

**The race:**

| Time | `pes commit` | GC |
|------|-------------|-----|
| t1 | `object_write(OBJ_BLOB)` stores blob B | — |
| t2 | — | Mark phase reads HEAD → old commit. B is not reachable from any ref. |
| t3 | `object_write(OBJ_TREE)` creates tree T referencing B | — |
| t4 | — | Sweep: B is not in reachable set → `unlink(B)` |
| t5 | `object_write(OBJ_COMMIT)` + `head_update` completes | — |

Result: HEAD now points to a commit whose tree references the deleted blob B.
The repository is corrupt.

**How Git avoids this:**

1. **Grace period (`gc.pruneExpire`, default 2 weeks).** GC skips any loose
   object whose filesystem `mtime` is younger than the grace period. A freshly
   written blob is always newer than two weeks, so it is never collected before
   it is referenced by a commit.
2. **Keep-alive refs.** Git writes temporary refs (`FETCH_HEAD`, `MERGE_HEAD`,
   `ORIG_HEAD`) during in-flight operations, so GC sees those objects as
   reachable roots.
3. **Pack-before-prune ordering.** When converting loose objects to pack files,
   Git writes and verifies the pack before deleting any loose objects, so there
   is never a window where an object exists only in a half-written pack.
4. The atomic rename used when writing objects does not help here — blob B is
   fully written before GC runs. Only the grace period closes the time window
   reliably.
