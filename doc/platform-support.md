# Nebase Platform Support

Nebase is supported mainly on Unix-like systems and support for other OS should
be added if they provide the corresponding APIs.

We only support 64bit architectures for now. But if convenient, the code should
also support 32bit architectures.

## Operation System

Nebase support levels are organized into three tiers, each with a different set
of guarantees.

### Tier 1

Tier 1 platforms are the *feature definition* platforms.

1. All features must be available.
2. All testcases must pass.
3. Code should be optimized.

| OS  |Notes|
|:---:|:----|
|Linux||
|FreeBSD||
|NetBSD||

### Tier 2

Tier 2 platforms are the *best effort* platforms.

1. All features should be available.
2. Most testcases should pass.

| OS  |Notes|
|:---:|:----|
|Solaris||
|Illumos||

### Tier 3

Tier 3 platforms are the *feature missing* platforms.

1. Feature should be added if available.
2. Testcases of available features should pass.

| OS  |Notes|
|:---:|:----|
|DragonFly BSD||
|OpenBSD||
|macOS||
|Haiku||
