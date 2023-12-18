{ callPackage
, http-parser
, lib
, libiconv
, openssl
, openssl_1_1
, pcre
, zlib
}:
let
  inherit (callPackage ./util.nix { }) mkStatic;
in
{
  http-parser = mkStatic http-parser;
  libiconv = mkStatic libiconv;
  openssl = mkStatic openssl;
  openssl_1_1 = mkStatic openssl_1_1;
  pcre = mkStatic pcre;
  # zlib.static only exists in `x86_64-darwin.pkgs.zlib`, after 
  # https://github.com/NixOS/nixpkgs/pull/256590 `pkgsStatic.zlib`
  # should work as expected.
  zlib = (zlib.static or zlib);
}
