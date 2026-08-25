// @category XeO3

import ghidra.app.decompiler.DecompInterface;
import ghidra.app.decompiler.DecompileResults;
import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.Address;
import ghidra.program.model.listing.Function;
import ghidra.program.model.symbol.Reference;
import ghidra.program.model.symbol.ReferenceIterator;

public final class GhidraDecompileTargets extends GhidraScript {
    @Override
    protected void run() throws Exception {
        if (currentProgram == null) {
            printerr("No current program");
            return;
        }

        println("PROGRAM " + currentProgram.getDomainFile().getPathname());
        println("IMAGE_BASE " + currentProgram.getImageBase());

        DecompInterface decompiler = new DecompInterface();
        decompiler.toggleCCode(true);
        decompiler.toggleSyntaxTree(true);
        if (!decompiler.openProgram(currentProgram)) {
            printerr("Decompiler initialization failed: " + decompiler.getLastMessage());
            return;
        }

        try {
            for (String argument : getScriptArgs()) {
                monitor.checkCancelled();
                Address address = toAddr(argument);
                println("TARGET " + address);

                ReferenceIterator references =
                    currentProgram.getReferenceManager().getReferencesTo(address);
                int referenceCount = 0;
                while (references.hasNext()) {
                    Reference reference = references.next();
                    Function fromFunction =
                        currentProgram.getFunctionManager().getFunctionContaining(
                            reference.getFromAddress());
                    println(
                        "XREF " + reference.getFromAddress() + " " +
                        reference.getReferenceType() + " " +
                        (fromFunction == null ? "<no-function>"
                                             : fromFunction.getName() + "@" +
                                                   fromFunction.getEntryPoint()));
                    referenceCount++;
                }
                println("XREF_COUNT " + referenceCount);

                Function function =
                    currentProgram.getFunctionManager().getFunctionContaining(address);
                if (function == null) {
                    println("NO_FUNCTION");
                    continue;
                }

                println(
                    "FUNCTION " + function.getName() + " " +
                    function.getEntryPoint() + " " + function.getBody());
                DecompileResults result =
                    decompiler.decompileFunction(function, 180, monitor);
                if (!result.decompileCompleted()) {
                    printerr(
                        "DECOMPILE_FAILED " + function.getEntryPoint() + " " +
                        result.getErrorMessage());
                    continue;
                }
                println("DECOMPILE_BEGIN " + function.getEntryPoint());
                println(result.getDecompiledFunction().getC());
                println("DECOMPILE_END " + function.getEntryPoint());
            }
        } finally {
            decompiler.dispose();
        }
    }
}
