with import <nixpkgs> {};
mkShell.override { stdenv = llvmPackages_12.stdenv; } {
    buildInputs = [
        linuxPackages.perf
        llvmPackages_12.lld
        openjdk
        shellcheck
        unzip
        wget
    ];
    shellHook = ''
        . .shellhook
    '';
}
