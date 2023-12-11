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
  http-parser = (mkStatic http-parser).overrideAttrs (oldAttrs: {
    # needed until upstream PR hits nixpkgs-unstable,
    # track https://nixpk.gs/pr-tracker.html?pr=254516.
    # http-parser makefile is a bit incomplete, so fill in the gaps here
    # to move the static object and header files to the right location
    # https://github.com/nodejs/http-parser/issues/310.
    buildFlags = [ "package" ];
    installTargets = "package";
    postInstall = ''
      install -D libhttp_parser.a $out/lib/libhttp_parser.a
      install -D  http_parser.h $out/include/http_parser.h
      ls -la $out/lib $out/include
    '';
  });

  libiconv = mkStatic libiconv;
  openssl = mkStatic openssl;
  openssl_1_1 = mkStatic openssl_1_1;
  pcre = mkStatic pcre;
  # zlib.static only exists in `x86_64-darwin.pkgs.zlib`, after 
  # https://github.com/NixOS/nixpkgs/pull/256590 `pkgsStatic.zlib`
  # should work as expected.
  zlib = (zlib.static or zlib);
}
