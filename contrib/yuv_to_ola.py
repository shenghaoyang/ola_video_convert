#!/usr/bin/env python3

import pexpect
import sys
import typing
import re
import logging
from dataclasses import dataclass
from pexpect import fdpexpect

T = typing.TypeVar("T")


header_re = re.compile(
    rb"GREY W(?P<size>\d+) H(?P<universes>\d+) F\d+:\d+ I[a-z] A\d+:\d+\n"
)


@dataclass(frozen=True)
class Universe:
    number: int
    data: bytes

    @classmethod
    def from_segment(cls: T, segment: bytes) -> T:
        number = int.from_bytes(segment[:2], byteorder="little")
        data = segment[2:]
        return cls(number=number, data=data)

    def dump_for_ola(self) -> str:
        return f"{self.number} {','.join((str(b) for b in self.data))}"


@dataclass(frozen=True)
class StreamMetadata:
    universes: typing.Optional[int] = None
    segment_size: typing.Optional[int] = None
    frame_re: typing.Optional[typing.Pattern[bytes]] = None

    @classmethod
    def from_header_match(cls: T, m: typing.Match[bytes]) -> T:
        universes = int(m["universes"].decode("utf-8"))
        segment_size = int(m["size"].decode("utf-8"))
        frame_re = re.compile(
            rf"FRAME\n(?P<frame>[\x00-\xff]{{{segment_size * universes}}})".encode(
                "utf-8"
            )
        )
        metadata = cls(
            universes=universes, segment_size=segment_size, frame_re=frame_re
        )

        return metadata

    @property
    def available(self) -> bool:
        return self.universes is not None


def main():
    metadata = StreamMetadata()
    yuv_in = fdpexpect.fdspawn(sys.stdin, timeout=None, use_poll=True)
    logging.basicConfig()
    logger = logging.getLogger(__name__)
    logger.setLevel(logging.INFO)

    try:
        while True:
            if not metadata.available:
                yuv_in.expect(header_re)
                metadata = StreamMetadata.from_header_match(yuv_in.match)
                logger.info(
                    "Got stream header: "
                    f"{metadata.universes} universe(s) at "
                    f"{metadata.segment_size} bytes per universe."
                )
            else:
                yuv_in.expect_list((header_re, metadata.frame_re))

                if (m := yuv_in.match).re is header_re:
                    metadata = StreamMetadata.from_header_match(m)
                elif m.re is metadata.frame_re:

                    frame = m["frame"]
                    universes = (
                        Universe.from_segment(
                            frame[
                                i
                                * metadata.segment_size : (i + 1)
                                * metadata.segment_size
                            ]
                        )
                        for i in range(metadata.universes)
                    )
                    out = " ".join(u.dump_for_ola() for u in universes)
                    print(out, flush=True)

    except pexpect.EOF:
        return 0


if __name__ == "__main__":
    exit(main())
