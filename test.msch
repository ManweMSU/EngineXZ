Subsystem = "Console"
Name = "test"
Data = binary { 66 77 88 11 22 33 44 99 00 }
Variables {
    NUMBER {
        TypeCN = "Cuint16"
        "Offset.B" = 1
        "Offset.W" = 0
        "Size.B" = 2
        "Size.W" = 0
        Attributes {
            TEST = "TEST"
        }
    }
}
Functions {
    "MAIN:CMAIN" {
        Code = "test2.asm"
        EntryPoint = true
        Throws = true
    }
    "PRINT:CPRINT" {
        Import = "print_integer"
    }
}
Resources {
    Metadata {
        NomenModuli = "Test Module"
        ModuliCreatoris = "Engine Software"
        ExemplumIura = "(C) Engine Software"
        Versio = "0.0.0.0"
    }
}