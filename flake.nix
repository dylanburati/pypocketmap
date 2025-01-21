{
  inputs = {
    nixpkgs.url = "github:nixos/nixpkgs/nixos-24.11";

    flake-utils.url = "github:numtide/flake-utils";

    proccorder.url = "github:dylanburati/proccorder";
  };

  outputs = { self, nixpkgs, flake-utils, proccorder, ... }:
    flake-utils.lib.eachDefaultSystem (system:
      let
        pkgs = nixpkgs.legacyPackages.${system};

        pypocketmap = pkgs.python312Packages.buildPythonPackage {
          name = "pypocketmap";
          src = ./.;
          propagatedBuildInputs = [ pkgs.python312Packages.setuptools ];
        };

        pythonDerivation = (pkgs.python312.withPackages (pyPkgs: with pyPkgs; [
          black
          (deal.overrideAttrs { doCheck = false; doInstallCheck = false; })
          hypothesis
          numpy
          tox
          typing-extensions

          pypocketmap
        ]));

        commonArgs = {
          buildInputs = [
            pkgs.gcc13
            pkgs.gnumake
            pythonDerivation
            proccorder.packages.${system}.default
          ] ++ pkgs.lib.optionals pkgs.stdenv.isDarwin [
            # Additional darwin specific inputs can be set here
            pkgs.libiconv
          ];
        };
      in
      {
        devShells.default = pkgs.mkShell {
          packages = with pkgs; [
            bear
            ccls
            pythonDerivation
            proccorder.packages.${system}.default
          ];

          shellHook = ''
            export CPATH="${pythonDerivation}/include/python3.12:$CPATH"
            export LIBRARY_PATH="${pythonDerivation}/lib:$LIBRARY_PATH"

            pypocketmap_bench () {
              mkdir -p ".profiles/''${TOX_SHA1:-latest}"
              proccorder -o=.profiles/''${TOX_SHA1:-latest}/proc_linux_mdict_sm.ldjson python ./tests/profile/wordcount.py mdict 1000000
              proccorder -o=.profiles/''${TOX_SHA1:-latest}/proc_linux_dict_sm.ldjson python ./tests/profile/wordcount.py dict 1000000
              proccorder -o=.profiles/''${TOX_SHA1:-latest}/proc_linux_mdict_md.ldjson python ./tests/profile/wordcount.py mdict 8000000
              proccorder -o=.profiles/''${TOX_SHA1:-latest}/proc_linux_dict_md.ldjson python ./tests/profile/wordcount.py dict 8000000
              proccorder -o=.profiles/''${TOX_SHA1:-latest}/proc_linux_mdict_lg.ldjson python ./tests/profile/wordcount.py mdict 32000000
              proccorder -o=.profiles/''${TOX_SHA1:-latest}/proc_linux_dict_lg.ldjson python ./tests/profile/wordcount.py dict 32000000
            }
          '';
        };

        packages.default = pypocketmap;
      });
}
