"""Combinatorial allocation and VCG settlement for fixed public resource menus."""

from .model import Auction, AuctionError, load_auction, parse_auction

__all__ = ["Auction", "AuctionError", "load_auction", "parse_auction"]
__version__ = "0.1.0"
