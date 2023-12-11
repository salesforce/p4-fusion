{ callPackage
, cmake
, darwin
, helix-core-api-set
, hostPlatform
, http-parser
, lib
, libiconv
, openssl
, overrideSDK
, patchelf
, pcre
, pkg-config
, stdenv
, zlib
}:
let
  inherit (callPackage ./util.nix { }) unNixifyDylibs;
  # at the time of writing, there is an SDK version issue when building on
  # x86_64-darwin, so we force it to 11.0.
  # in future nixpkgs version, SDK versions should be better and more
  #  consistently configured.
  stdenv' = (if stdenv.isDarwin then overrideSDK stdenv "11.0" else stdenv);
in
unNixifyDylibs (stdenv'.mkDerivation rec {
  name = "p4-fusion";
  version = "v1.13.2-sg";

  srcs = [
    (lib.sources.cleanSource ../.)
    (helix-core-api-set.${hostPlatform.system})
  ];

  sourceRoot = "source";

  # copy helix-core-api stuff into the expected directories, and statically link libstdc++
  postUnpack = let dir = if hostPlatform.isMacOS then "mac" else "linux"; in ''
    mkdir -p $sourceRoot/vendor/helix-core-api/${dir}
    cp -R helix-core-api/* $sourceRoot/vendor/helix-core-api/${dir}
  ''
  # some extra cmake instructions to statically link libstdc++
  + lib.optionalString hostPlatform.isStatic ''
    substituteInPlace $sourceRoot/p4-fusion/CMakeLists.txt \
      --replace 'target_link_libraries(p4-fusion PUBLIC' \
                'target_link_libraries(p4-fusion PUBLIC -static-libstdc++'
  '';

  # on macos only it doesnt pick up sqlite + curl (and also depends on some lua stuff that it can't find?)
  # so we'll just use the p4 provided ones. Passing `-lsqlite3 -lcurl` solves sqlite and curl, but not lua.
  postPatch = lib.optionalString hostPlatform.isMacOS ''
    substituteInPlace p4-fusion/CMakeLists.txt \
      --replace 'p4script_c' 'p4script_c p4script_sqlite p4script_curl'
  '';

  nativeBuildInputs = [
    patchelf
    pkg-config
    cmake
  ];

  buildInputs = [
    zlib
    http-parser
    pcre
    openssl
  ] ++ lib.optionals hostPlatform.isMacOS [
    # iconv is bundled with glibc and apparently only needed for osx
    # https://sourcegraph.com/github.com/sourcegraph/p4-fusion@82289290c68a3d2b5d3f4adc9db1cadd686cfcef/-/blob/vendor/libgit2/README.md?L178:3
    libiconv
    darwin.apple_sdk.frameworks.CFNetwork
    darwin.apple_sdk.frameworks.Cocoa
  ];

  cmakeFlags = [
    # Copied from upstream, where relevant
    # https://sourcegraph.com/github.com/sourcegraph/p4-fusion@82289290c68a3d2b5d3f4adc9db1cadd686cfcef/-/blob/generate_cache.sh?L7-21
    "-DUSE_SSH=OFF"
    "-DUSE_HTTPS=OFF"
    "-DBUILD_CLAR=OFF"
    # salesforce don't link against GSSAPI in CI, so I won't either
    "-DUSE_GSSAPI=OFF"
    # prefer nix-provided http-parser instead of bundled
    "-DUSE_HTTP_PARSER=system"
  ] ++ lib.optional hostPlatform.isStatic "-DBUILD_SHARED_LIBS=OFF";

  postInstall = ''
    mkdir -p $out/bin
    cp p4-fusion/p4-fusion $out/bin/p4-fusion
    ln -s $out/bin/p4-fusion $out/bin/p4-fusion-${version}
  '';

  meta = {
    homepage = "https://github.com/sourcegraph/p4-fusion";
    platforms = [ "x86_64-darwin" "aarch64-darwin" "x86_64-linux" ];
    license = lib.licenses.bsd3;
  };
})
