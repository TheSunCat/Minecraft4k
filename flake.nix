{
  description = "Flake to dev Minecraft4k";

  inputs = {
    nixpkgs.url = "https://flakehub.com/f/NixOS/nixpkgs/0.2405.*.tar.gz";
  };

  outputs = {
    self,
    nixpkgs,
  }: let
    name = "Minecraft4k";
    system = "x86_64-linux";
    pkgs = nixpkgs.legacyPackages.${system};

    tools = [
      pkgs.gcc
      pkgs.elfkickers
      pkgs.mono
      pkgs.nasm
    ];

    deps = [
      pkgs.SDL2
      pkgs.libGL
    ];
  in {
    devShells.${system}.default = pkgs.mkShell {
      name = "${name}-dev";

      packages = tools;
      buildInputs = deps;

      LD_LIBRARY_PATH = pkgs.lib.makeLibraryPath [pkgs.SDL2 pkgs.libGL];
    };

    packages.${system}.default = pkgs.stdenv.mkDerivation {
      inherit name;

      src = self;
      nativeBuildInputs = tools;
      buildInputs = deps;

      installPhase = ''
        mkdir -p $out/bin
        cp ${name} $out/bin
      '';
    };
  };
}
