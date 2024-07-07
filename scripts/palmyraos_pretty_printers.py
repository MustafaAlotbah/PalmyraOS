import gdb


class StringPrinter:
    "Print a PalmyraOS::types::string"

    def __init__(self, val):
        self.val = val

    def to_string(self):
        # Access the 'cstr' member
        cstr = self.val['cstr']
        # Format the output to look like a string
        return '"' + cstr.string() + '"'


class DentryPrinter:
    "Print a PalmyraOS::types::string"

    def __init__(self, val):
        self.val = val

    def to_string(self):
        # Access the 'cstr' member
        name = self.val['name_']
        inode = self.val['inode_']
        # Format the output to look like a string
        return '"' + name.string() + '" ' + inode.string()


class PairPrinter:
    "Print a std::pair"

    def __init__(self, val):
        self.val = val

    def to_string(self):
        first = self.val['first']
        second = self.val['second']
        # Format the output to display the pair with some colors
        return f'({first}) -> ({second})'


def lookup_function(val):
    type_str = str(val.type.strip_typedefs())
    if type_str.startswith("PalmyraOS::types::string<char,"):
        return StringPrinter(val)
    if type_str.startswith("const PalmyraOS::kernel::KString"):
        return StringPrinter(val)
    if type_str.startswith("PalmyraOS::kernel::KString"):
        return StringPrinter(val)
    if type_str.startswith("PalmyraOS::kernel::vfs::Dentry"):
        return DentryPrinter(val)
    if type_str.startswith("std::pair<"):
        return PairPrinter(val)
    return None


def register_printers(obj):
    if obj is None:
        obj = gdb
    obj.pretty_printers.append(lookup_function)


# Register the pretty-printers
register_printers(gdb.current_objfile())
