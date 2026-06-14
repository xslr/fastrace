import sys
import argparse
from collections import Counter
from vblf.reader import BlfReader
from vblf.constants import ObjType

def count_messages(file_path):
    message_types = {
        ObjType.CAN_MESSAGE,
        ObjType.CAN_MESSAGE2,
        ObjType.CAN_FD_MESSAGE,
        ObjType.CAN_FD_MESSAGE_64,
        ObjType.ETHERNET_FRAME,
        ObjType.ETHERNET_FRAME_EX,
    }

    # Build a reverse map: integer value -> enum name (for unknown types too)
    obj_type_by_value = {member.value: member for member in ObjType}

    count = 0
    total_objects = 0
    type_counts: Counter = Counter()
    try:
        with BlfReader(file_path) as reader:
            for obj in reader:
                total_objects += 1
                obj_type = obj.header.base.object_type
                type_counts[obj_type] += 1
                if obj_type in message_types:
                    count += 1
    except Exception as e:
        print(f"Error reading file: {e}")
        return

    print(f"File: {file_path}")
    print(f"Total BLF Objects: {total_objects}")
    print(f"Total Trace Messages: {count}")

    print("--- Decoded objects (by type, sorted by count) ---")
    if not type_counts:
        print("  (none)")
    else:
        for obj_type, cnt in type_counts.most_common():
            # obj_type may be an ObjType enum or a raw int
            raw = obj_type.value if isinstance(obj_type, ObjType) else int(obj_type)
            member = obj_type_by_value.get(raw)
            name = member.name if member is not None else f"UNKNOWN_{raw}"
            print(f"  [{raw:>3}] {name:<36} : {cnt}")

if __name__ == "__main__":
    parser = argparse.ArgumentParser(description="Verify the number of messages in a BLF trace file")
    parser.add_argument("file_path", help="Path to the BLF file")
    args = parser.parse_args()

    count_messages(args.file_path)
