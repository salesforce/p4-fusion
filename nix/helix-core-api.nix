{ fetchzip }:
{
  "1.1" = {
    aarch64-darwin = fetchzip {
      name = "helix-core-api";
      url = "https://cdist2.perforce.com/perforce/r23.1/bin.macosx12arm64/p4api-openssl1.1.1.tgz";
      hash = "sha256-MiRKvW/IbA1AcNpFWCIBGsYomelCLrTU04+Vq5yyUVI=";
    };
    x86_64-darwin = fetchzip {
      name = "helix-core-api";
      url = "https://cdist2.perforce.com/perforce/r23.1/bin.macosx12x86_64/p4api-openssl1.1.1.tgz";
      hash = "sha256-QF69oWI8qqST9ELJrEzx4pFQik/RsEmsRVq7EOo5gt0=";
    };
    x86_64-linux = fetchzip {
      name = "helix-core-api";
      url = "https://cdist2.perforce.com/perforce/r23.1/bin.linux26x86_64/p4api-glibc2.3-openssl1.1.1.tgz";
      hash = "sha256-R8oP1uJ70J2XRToLAPAVKxU/VEr2e6LlaR3zn21lKaI=";
    };
  };
  "3.0" = {
    aarch64-darwin = fetchzip {
      name = "helix-core-api";
      url = "https://cdist2.perforce.com/perforce/r23.1/bin.macosx12arm64/p4api-openssl3.tgz";
      hash = "sha256-SVAw1ADhd7pwgHprE+J61bp5AiP9ULCw64WeN5MVV9M=";
    };
    x86_64-darwin = fetchzip {
      name = "helix-core-api";
      url = "https://cdist2.perforce.com/perforce/r23.1/bin.macosx12x86_64/p4api-openssl3.tgz";
      hash = "sha256-HhSmQWfX9yh734FtG14BzIQY9YliAH9kV5RaQ71T0RU=";
    };
    x86_64-linux = fetchzip {
      name = "helix-core-api";
      url = "https://cdist2.perforce.com/perforce/r23.1/bin.linux26x86_64/p4api-glibc2.3-openssl3.tgz";
      hash = "sha256-oJzHuq8/9PgGi6e1/bxXuaZER5+DR5tmCIN21/fVImQ=";
    };
  };
}
