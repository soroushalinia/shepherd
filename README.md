shepherd
========

shepherd is a Linux kernel module that places a single opinionated sheep
inside `/proc/sheep`. It serves no purpose beyond reminding you that not
every kernel module has to be serious. Feed it, pet it, scare it, and
discover a few hidden surprises.

> **You're the shepherd. Not the sheep.**

---

Quick Start
-----------

```console
$ make
$ sudo insmod shepherd.ko
$ cat /proc/sheep
🐑 Baaaa.
$ echo feed > /proc/sheep; cat /proc/sheep
🐑 nom nom nom
$ sudo rmmod shepherd
```

> **Security note (the fun kind):** `/proc/sheep` is world-writable (0666)
> on purpose — the pasture has no fences. Any user on the system can pet,
> feed, or scare the sheep. The kernel will let you know (see dmesg).
> The sheep are now self-governing.

Commands
--------

| Command        | Response                                       |
|----------------|------------------------------------------------|
| `pet`          | 🐑 *wags tail*                                 |
| `feed`         | 🐑 nom nom nom                                 |
| `sleep`        | 🐑 zzz...                                      |
| `wake`         | 🐑 Baa?                                        |
| `wolf`         | 🐑 AAAAAAAAAAAAAAAAAAA                         |
| `calm`         | 🐑 *breathes slowly*                           |
| `whoami`       | You are the shepherd.                          |
| `call`         | 🐑 🐑 🐑 🐑 🐑 / The flock gathers.            |
| `count`        | 1 sheep. / Still enough to start a flock.      |
| `search`       | (1% chance) You found the lost sheep.          |
| `shear`        | 🐑 It's a little cold now.                     |
| `psalm23`      | The shepherd shall not segfault.               |
| `countsheep`   | 1... 2... 3... You feel sleepy.                |
| `42`           | The answer is 42 sheep.                        |
| `cve`          | CVE-2026-SHEEP-0001: Improper access control.  |

Easter eggs
-----------

| Trigger                  | Message                                        |
|--------------------------|------------------------------------------------|
| 1000 reads               | The sheep finally trusts you.                  |
| 100 consecutive `pet`s   | The sheep politely asks for personal space.    |
| Unloading the module     | Goodbye, shepherd. Take care of your flock.    |

Building
--------

**Requirements:** Linux kernel headers for your running kernel.

| Distribution | Install command                          |
|--------------|------------------------------------------|
| Arch Linux   | `sudo pacman -S linux-headers`           |
| Debian/Ubuntu| `sudo apt install linux-headers-$(uname -r)` |
| Fedora       | `sudo dnf install kernel-devel`          |
| openSUSE     | `sudo zypper install kernel-devel`       |

```console
$ make
$ sudo make modules_install
```

Or without install:

```console
$ make
$ sudo insmod shepherd.ko
$ sudo rmmod shepherd
```

Uninstall
---------

```console
$ sudo make modules_uninstall
$ sudo rm -f /lib/modules/$(uname -r)/extra/shepherd.ko
```

Kernel log
----------

```console
$ dmesg | grep shepherd
shepherd: a lone sheep appears in /proc/sheep
shepherd: SECURITY WARNING: pasture permissions are 0666.
shepherd: the sheep are now self-governing.
shepherd: CVE-2026-SHEEP-0001 triggered (uid 1000): unauthorized shepherd detected.
shepherd: wolf detected!
shepherd: Goodbye, shepherd.
shepherd: Take care of your flock.
```

CVE-2026-SHEEP-0001
--------------------

**Description:** Improper access control in Shepherd allows unprivileged
users to pet the sheep.

**Severity:** 🟢 Informational

**Impact:** Excessive baaing. The sheep have accepted their new leader.

**Mitigation:** Build a fence (`#define PROC_MODE 0660`).

State machine
-------------

The sheep has six states that change based on your actions and the
passage of time:

```
                   ┌─────────┐
                   │ NORMAL  │◄──────────────────┐
                   └────┬────┘                   │
          ┌─────────────┼─────────────┐           │
          ▼             ▼             ▼           │
      ┌──────┐    ┌──────────┐   ┌────────┐      │
      │HAPPY │    │  SCARED  │   │ SLEEP  │      │
      └──┬───┘    └────┬─────┘   └───┬────┘      │
         │ ───────────>│ 30s         │           │
         ▼             │             ▼           │
      ┌────────┐      │          ┌─────────┐    │
      │ HUNGRY │◄─────┘          │ IGNORED │────┘
      └────────┘     60s         └─────────┘
```

- **NORMAL:** Default state. Content but unremarkable.
- **HAPPY:** After being petted or fed.
- **HUNGRY:** 30 seconds without food.
- **SLEEPING:** Put to bed with `sleep`. Stays asleep until `wake`.
- **SCARED:** After a wolf encounter. Stays scared until `calm`.
- **IGNORED:** 60 seconds without any interaction.

Architecture
------------

```
        ┌─────────────────────────────────────────┐
        │            kernel space                  │
        │                                          │
        │   sheperd.ko                             │
        │   ┌────────────────────────────────┐     │
        │   │  /proc/sheep                   │     │
        │   │  ┌─────────┐   ┌───────────┐  │     │
        │   │  │  READ   │   │  WRITE    │  │     │
        │   │  │  handler│   │  handler  │  │     │
        │   │  └────┬────┘   └─────┬─────┘  │     │
        │   │       │              │         │     │
        │   │       ▼              ▼         │     │
        │   │  ┌────────────────────────┐    │     │
        │   │  │    sheep state         │    │     │
        │   │  │  (normal / happy /     │    │     │
        │   │  │   hungry / sleeping /  │    │     │
        │   │  │   scared / ignored)    │    │     │
        │   │  └────────────────────────┘    │     │
        │   └────────────────────────────────┘     │
        │                                          │
        └──────────────────────────────────────────┘
                           │
                    ┌──────┴──────┐
                    ▼             ▼
              echo cat >      cat /proc/sheep
```

Files
-----

| File           | Description                        |
|----------------|------------------------------------|
| `src/shepherd.c`| Kernel module source              |
| `Makefile`     | Kernel module build system         |
| `README.md`    | This file                          |
| `build/`       | Build output (generated)           |

License
-------

GNU General Public License v2.0 only. See [COPYING](COPYING) for details.
