{
  inputs.nixpkgs.url = "github:NixOS/nixpkgs/nixos-unstable";

  outputs = { self, nixpkgs }: let
    systems = [ "x86_64-linux" "aarch64-linux" ];
    forEach = f: nixpkgs.lib.genAttrs systems (system: f (import nixpkgs { inherit system; }));
  in {
    packages = forEach (pkgs: rec {
      default = framexr;
      framexr = pkgs.callPackage ./nix/package.nix {
        src = self;
        useSystemDeps = true;
      };
      framexr-vendored = pkgs.callPackage ./nix/package.nix { src = self; };
    });

    devShells = forEach (pkgs: {
      default = pkgs.mkShell {
        inputsFrom = [ self.packages.${pkgs.system}.default ];
        packages = with pkgs; [ meson ninja clang-tools gdb ];
      };
    });

    checks = forEach (pkgs: { build = self.packages.${pkgs.system}.default; });
    formatter = forEach (pkgs: pkgs.alejandra);
  };
}
