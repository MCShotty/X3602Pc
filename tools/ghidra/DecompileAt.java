// Decompile the function containing a supplied address from a headless Ghidra run.
// @category XeO3

import java.io.File;
import java.nio.charset.StandardCharsets;
import java.nio.file.Files;

import ghidra.app.decompiler.DecompInterface;
import ghidra.app.decompiler.DecompileResults;
import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.Address;
import ghidra.program.model.listing.Function;

public class DecompileAt extends GhidraScript {
    @Override
    protected void run() throws Exception {
        String[] arguments = getScriptArgs();
        if (arguments.length != 2 && arguments.length != 3) {
            throw new IllegalArgumentException(
                "Usage: DecompileAt <address> <output-file> [expected-executable-sha256]");
        }

        String executableHash = currentProgram.getExecutableSHA256();
        if (arguments.length == 3 &&
            !arguments[2].equalsIgnoreCase(executableHash)) {
            throw new IllegalStateException("Executable SHA-256 mismatch: " + executableHash);
        }

        Address address = currentProgram.getAddressFactory()
            .getDefaultAddressSpace().getAddress(arguments[0]);
        Function function = getFunctionContaining(address);
        if (function == null) {
            function = getFunctionAt(address);
        }
        if (function == null) {
            throw new IllegalStateException("No function contains " + address);
        }

        DecompInterface decompiler = new DecompInterface();
        decompiler.openProgram(currentProgram);
        try {
            DecompileResults results = decompiler.decompileFunction(
                function, 120, monitor);
            if (!results.decompileCompleted()) {
                throw new IllegalStateException(results.getErrorMessage());
            }
            String header = String.format(
                "// %s at %s%n// Executable SHA-256: %s%n%n",
                function.getName(), function.getEntryPoint(), executableHash);
            Files.writeString(new File(arguments[1]).toPath(),
                header + results.getDecompiledFunction().getC(),
                StandardCharsets.UTF_8);
        } finally {
            decompiler.dispose();
        }
    }
}
