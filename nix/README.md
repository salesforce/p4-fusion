# Nix for p4-fusion

This Nix-based infrastructure aims to provide us with reproducible, deterministic and hermetic binary builds for provisioning with Bazel over in [sourcegraph/sourcegraph](https://github.com/sourcegraph/sourcegraph).

Github Actions workflows are used to build and upload the binaries.

**Note**: please reach out in [#discuss-dev-infra](https://sourcegraph.slack.com/archives/C04MYFW01NV) if you're experiencing issues or uncertainty with or just wanna chat about any of this, either locally or in Github Actions.

We highly recommend the [Determinate Systems' Nix installer](https://github.com/DeterminateSystems/nix-installer) if you want/need to use or modify any of this locally.

## Binary targets

Build targets are provided for various combinations of target OS, architecture, OpenSSL version and static vs dynamic binaries. To build e.g. a static x86_64 Linux with OpenSSL 1.1, you would run `nix build .#p4-fusion_openssl1_1-static` on a Linux machine. Check the `packages` section of `nix flake show` for the full list of targets.

## Troubleshooting

### nix build fails with "hash mismatch" error referencing helix-core-api.drv

In `./nix/helix-core-api.nix`, we specify the URLs for the helix-core-api dependency for different OpenSSL and system/arch combinations, along with a checksum for those archives (as required by Nix). Sometimes those URLs are re-used for updated archives of helix-core-api, which will cause a checksum mismatch similar to the following:

```go
building '/nix/store/cb2ssx8vykl1ghb4k87yp3q6wnvfvjj2-helix-core-api.drv'...
error: hash mismatch in fixed-output derivation '/nix/store/cb2ssx8vykl1ghb4k87yp3q6wnvfvjj2-helix-core-api.drv':
         specified: sha256-8yX9sjE1f798ns0UmHXz5I8kdpUXHS01FG46SU8nsZw=
            got:    sha256-gaYvQOX8nvMIMHENHB0+uklyLcmeXT5gjGGcVC9TTtE=
error: 1 dependencies of derivation '/nix/store/m409z1rq40bwzvvndbnghrrxm000zd9v-p4-fusion.drv' failed to build
```

We have provided a runnable invoked with `nix run .#print-checksums` to output the set of checksums to replace the existing ones with. These can be added into the relevant string fields of `./nix/helix-core-api.nix`.

To check that the checksums are valid, you can run `nix run .#check-checksums`, which will quickly error out as above if any mismatches occur.

### nix build fails while compiling/linking p4-fusion or any dependencies

Please reach out to [#discuss-dev-infra](https://sourcegraph.slack.com/archives/C04MYFW01NV) to have someone take over (or pair with you if you're interested). If you want to take a stab at it (for the dopamine hit), be sure to not spend too long at it before asking for help!
