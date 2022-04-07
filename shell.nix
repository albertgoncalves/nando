with import <nixpkgs> {};
mkShell.override { stdenv = llvmPackages_12.stdenv; } {
    buildInputs = [
        linuxPackages.perf
        mold
        openjdk
        shellcheck
        unzip
        wget
    ];
    shellHook = ''
        . .shellhook
    '';
}
