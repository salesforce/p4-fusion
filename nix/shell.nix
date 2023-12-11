{ lib
, zlib
, http-parser
, pcre
, openssl
, libiconv
, mkShell
, hostPlatform
, patchelf
, cmake
, pkg-config
, darwin
}:
mkShell {
  name = "p4-fusion";

  nativeBuildInputs = [
    pkg-config
    cmake
    zlib
    http-parser
    pcre
    openssl
    patchelf
  ] ++ lib.optional hostPlatform.isMacOS [
    libiconv
    darwin.apple_sdk.frameworks.CFNetwork
    darwin.apple_sdk.frameworks.Cocoa
  ];
}
