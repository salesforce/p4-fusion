{ pkgs, lib }:
let
  helix-core-api-set = pkgs.callPackage ./helix-core-api.nix { };
  # we could provide printf/echo and nix/nix-prefetch-url here, but these are surely installed.
  echo-hash = system: openssl_version: url: ''
    HASH=$(nix hash to-sri "sha256:$(nix-prefetch-url --type sha256 --unpack ${url} 2>/dev/null)")
    printf '%15s %s: %s\n' "${system}" "openssl ${openssl_version}" $HASH
  '';
in
''
  echo 'Update the relevant hashes in nix/helix-core-api.nix with these values:'
'' + lib.concatLines (
  lib.mapAttrsToList (name: value: echo-hash name "1.1" value.url) helix-core-api-set."1.1"
  ++ lib.mapAttrsToList (name: value: echo-hash name "3.0" value.url) helix-core-api-set."3.0"
)
