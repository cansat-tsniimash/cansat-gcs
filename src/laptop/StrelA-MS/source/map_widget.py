from source.map_source.open_street_map import *

from source import settings_control


class MapWidget(OpenStreetMap):
    def __init__(self):
        super(MapWidget, self).__init__()
        self.settings = settings_control.init_settings()

        self.key = None
        self.data_len = 0
        self.overflow = False
        self.packet_name = 0
        self.follow = 0
        self.max_data_length = 0
        self.is_load_finished = False

        self.loadFinished.connect(self._on_load_finished)

    def setup_ui_design(self):
        self.settings.beginGroup("CentralWidget/MapWidget")
        if self.is_load_finished:
            self.set_center(*self.settings.value("center"))
            self.set_zoom(self.settings.value("zoom"))
        self.sourse_id = self.settings.value("sourse_id")
        self.message_id = self.settings.value("message_id")
        self.field_lat_id = self.settings.value("field_lat_id")
        self.field_lon_id = self.settings.value("field_lon_id")
        self.follow = (self.settings.value("follow") != False)
        self.max_data_length = self.settings.value("max_data_length")
        self.settings.endGroup()

    def update_current_values (self):
        pass

    def _on_load_finished(self):
        self.is_load_finished = True
        self.setup_ui_design()
        self.update_current_values()

    def new_data_reaction(self, data):
        points = []
        for msg in data:
            if (msg.get_source_id() == self.sourse_id) and (msg.get_message_id() == self.message_id):
                lat = msg.get_data_dict().get(self.field_lat_id, None)
                lon = msg.get_data_dict().get(self.field_lon_id, None)
                if (lat is not None) and (lon is not None):
                    points.append([lat, lon])

        if len(points) > 0:
            if self.key is None:
                self.key = 0
                self.add_marker(self.key, points[-1][0], points[-1][1], **dict())
                self.add_polyline(self.key, points, **dict(color="red"))
            else:
                self.move_marker(self.key, points[-1][0], points[-1][1])
                self.add_points_to_polyline(self.key, points)

            if self.follow:
                self.set_center(points[-1][0], points[-1][1])

            if self.max_data_length != 0:
                if self.overflow:
                    self.delete_first_n_points(self.key, len(points))
                else:
                    self.data_len = self.data_len + len(points)
                    if self.data_len > self.max_data_length:
                        self.delete_first_n_points(self.key, self.data_len - self.max_data_length)
                        self.data_len = self.max_data_length
                        self.overflow = True

    def clear_data(self):
        if self.key is not None:
            self.delete_marker(self.key)
            self.delete_polyline(self.key)
            self.key = None
            self.data_len = 0
            self.overflow = False