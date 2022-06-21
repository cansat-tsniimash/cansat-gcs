from itertools import tee
from collections import namedtuple
from threading import Lock

from shapely.geometry import Polygon, LineString, LinearRing

from fastkml import kml, Placemark
from fastkml.styles import PolyStyle, LineStyle, Style
from fastkml.geometry import Geometry


Point = namedtuple("Point", ["lon", "lat", "alt"])

KML_NS = '{http://www.opengis.net/kml/2.2}'
DOC_NAME = "Траектория зонда \"СНЕГИРЬ МС\""


def pairwise(iterable):
    """ s -> (s0,s1), (s1,s2), (s2, s3), ..."""
    a, b = tee(iterable)
    next(b, None)
    return zip(a, b)


class Dataset:
    def __init__(self):
        self.points = []
        self.kml = None
        self.update_needed = True
        self.lock = Lock()

    def add_point(self, lon, lat, alt):
        with self.lock:
            self.points.append(Point(lon=lon, lat=lat, alt=alt))
            self.update_needed = True

    def get_string(self):
        with self.lock:
            update_needed = self.update_needed

        if update_needed:
            self.kml = self.make_kml()

        return self.kml

    def make_shadow_polys(self, points):
        polygons = []
        for prev, cur in pairwise(points):
            ring = LinearRing([
                prev,
                cur,
                Point(cur.lon, cur.lat, 0),
                Point(prev.lon, cur.lat, 0),
                prev,
            ])
            poly = Polygon(ring)
            polygons.append(poly)

        return polygons

    def make_kml(self):
        with self.lock:
            points = list(self.points)

        shadow_style = Style(KML_NS, "s_shadow", styles=[PolyStyle(color='ccccccaa', outline=0)])
        trajectory_style = Style(KML_NS, "s_traj", styles=[LineStyle(color='ff0000ff', width=10)])

        kml_object = kml.KML(KML_NS)
        kml_document = kml.Document(
            KML_NS, "document_id", DOC_NAME, "", styles=[shadow_style, trajectory_style]
        )
        kml_object.append(kml_document)

        if len(points) > 1:
            traj_folder = kml.Folder(KML_NS, "f_traj", "Траектория", styleUrl="#s_traj")
            traj_placemark = Placemark(KML_NS, styleUrl="#s_traj")
            traj_placemark.geometry = LineString(points)
            traj_folder.append(traj_placemark)
            kml_document.append(traj_folder)

        shadow_polys = self.make_shadow_polys(points)
        if shadow_polys:
            shadow_folder = kml.Folder(KML_NS, "f_shadow", "Тень")
            kml_document.append(shadow_folder)
            for poly in shadow_polys:
                shadow_placemark = Placemark(KML_NS, styleUrl="#s_shadow")
                shadow_placemark.geometry = Geometry(
                    ns=KML_NS,
                    geometry=poly,
                    altitude_mode='relativeToGround'
                )
                shadow_folder.append(shadow_placemark)

        return kml_object.to_string()
