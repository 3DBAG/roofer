#ifdef RF_USE_RERUN
// Adapters so we can log eigen vectors as rerun positions:
template <>
struct rerun::CollectionAdapter<rerun::Position3D, roofer::PointCollection> {
  /// Borrow for non-temporary.
  Collection<rerun::Position3D> operator()(
      const roofer::PointCollection& container) {
    return Collection<rerun::Position3D>::borrow(container.data(),
                                                 container.size());
  }

  // Do a full copy for temporaries (otherwise the data might be deleted when
  // the temporary is destroyed).
  Collection<rerun::Position3D> operator()(
      roofer::PointCollection&& container) {
    std::vector<rerun::Position3D> positions(container.size());
    memcpy(positions.data(), container.data(),
           container.size() * sizeof(roofer::arr3f));
    return Collection<rerun::Position3D>::take_ownership(std::move(positions));
  }
};
template <>
struct rerun::CollectionAdapter<rerun::Position3D, roofer::TriangleCollection> {
  /// Borrow for non-temporary.
  Collection<rerun::Position3D> operator()(
      const roofer::TriangleCollection& container) {
    return Collection<rerun::Position3D>::borrow(container[0].data(),
                                                 container.vertex_count());
  }

  // Do a full copy for temporaries (otherwise the data might be deleted when
  // the temporary is destroyed).
  Collection<rerun::Position3D> operator()(
      roofer::TriangleCollection&& container) {
    std::vector<rerun::Position3D> positions(container.size());
    memcpy(positions.data(), container[0].data(),
           container.vertex_count() * sizeof(roofer::arr3f));
    return Collection<rerun::Position3D>::take_ownership(std::move(positions));
  }
};
#endif
