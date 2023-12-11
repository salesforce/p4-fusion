{
  description = "Flake for building p4-fusion with openssl1.1 and openssl3, dynamically and statically";

  inputs = {
    nixpkgs.url = "github:NixOS/nixpkgs/nixpkgs-unstable";
  };

  outputs = { self, nixpkgs }:
    let
      forAllSystems = fn:
        nixpkgs.lib.genAttrs [
          "x86_64-linux"
          "x86_64-darwin"
          "aarch64-darwin"
        ]
          (system: fn (import nixpkgs {
            config.permittedInsecurePackages = [ "openssl-1.1.1w" ];
            inherit system;
          }));
    in
    {
      packages = forAllSystems (pkgs:
        let
          helix-core-api-set = pkgs.callPackage ./nix/helix-core-api.nix { };
          # pkgsStatic on x86_64-darwin is not yet functional,
          # tracked in https://github.com/NixOS/nixpkgs/pull/256590 as static building
          # is cross-compilation under the hood.
          pkgsStatic = if pkgs.system == "x86_64-darwin" then pkgs else pkgs.pkgsStatic;
          # until the above is resolved, we need these static workarounds for x86_64-darwin,
          # but apply them to every system type as it doesn't harm them anyway.
          # note also a http-parser workaround detailed in the below file.
          static-deps-set = pkgsStatic.callPackage ./nix/static-deps.nix { };
        in
        {
          p4-fusion_openssl3 = pkgs.callPackage ./nix/binary.nix {
            helix-core-api-set = helix-core-api-set."3.0";
          };

          p4-fusion_openssl1_1 = pkgs.callPackage ./nix/binary.nix {
            openssl = pkgs.openssl_1_1;
            helix-core-api-set = helix-core-api-set."1.1";
          };

          p4-fusion_openssl3-static = pkgsStatic.callPackage ./nix/binary.nix {
            inherit (static-deps-set) http-parser libiconv openssl pcre zlib;
            helix-core-api-set = helix-core-api-set."3.0";
          };

          p4-fusion_openssl1_1-static = pkgsStatic.callPackage ./nix/binary.nix {
            openssl = static-deps-set.openssl_1_1;
            inherit (static-deps-set) http-parser libiconv pcre zlib;
            helix-core-api-set = helix-core-api-set."1.1";
          };
        });

      apps = forAllSystems (pkgs:
        let
          helix-core-api-set = pkgs.callPackage ./nix/helix-core-api.nix { };
          checksum-printer = pkgs.writeShellScript "checksum-printer" (pkgs.callPackage ./nix/checksum-printer-script.nix { });
          checksum-checker = pkgs.writeShellApplication {
            name = "checksum-checker";
            runtimeInputs = (builtins.attrValues helix-core-api-set."1.1") ++ (builtins.attrValues helix-core-api-set."3.0");
            text = "echo 'All checksums are good!'";
          };
        in
        {
          print-checksums = {
            program = "${checksum-printer}";
            type = "app";
          };
          check-checksums = {
            program = "${checksum-checker}/bin/checksum-checker";
            type = "app";
          };
        });

      devShells = forAllSystems (pkgs: { default = pkgs.callPackage ./nix/shell.nix { }; });

      formatter = forAllSystems (pkgs: pkgs.nixpkgs-fmt);
    };
}
