#ifndef BOTTOMLEFT_HPP
#define BOTTOMLEFT_HPP

#include <limits>

#include "placer_boilerplate.hpp"

namespace binpack2d { namespace strategies {

template<class RawShape>
struct BLConfig {
    TCoord<TPoint<RawShape>> min_obj_distance = 0;
    bool allow_rotations = false;
};

template<class RawShape>
class _BottomLeftPlacer: public PlacerBoilerplate<
        _BottomLeftPlacer<RawShape>,
        RawShape, _Box<TPoint<RawShape>>,
        BLConfig<RawShape> >
{
    using Base = PlacerBoilerplate<_BottomLeftPlacer<RawShape>, RawShape,
                                    _Box<TPoint<RawShape>>, BLConfig<RawShape>>;
    DECLARE_PLACER(Base)

public:

    explicit _BottomLeftPlacer(const BinType& bin): Base(bin) {}

    PackResult trypack(Item& item) {
        auto r = _trypack(item);
        if(!r && Base::config_.allow_rotations) {
            item.rotate(Degrees(90));
            r =_trypack(item);
        }
        return r;
    }

    enum class Dir {
        LEFT,
        DOWN
    };

    inline RawShape leftPoly(const Item& item) const {
        return toWallPoly(item, Dir::LEFT);
    }

    inline RawShape downPoly(const Item& item) const {
        return toWallPoly(item, Dir::DOWN);
    }

    inline Unit availableSpaceLeft(const Item& item) {
        return availableSpace(item, Dir::LEFT);
    }

    inline Unit availableSpaceDown(const Item& item) {
        return availableSpace(item, Dir::DOWN);
    }

protected:

    PackResult _trypack(Item& item) {

        // Get initial position for item in the top right corner
        setInitialPosition(item);

        Unit d = availableSpaceDown(item);
        bool can_move = d > 1 /*std::numeric_limits<Unit>::epsilon()*/;
        bool can_be_packed = can_move;
        bool left = true;

        while(can_move) {
            if(left) { // write previous down move and go down
                item.translate({0, -d+1});
                d = availableSpaceLeft(item);
                can_move = d > 1/*std::numeric_limits<Unit>::epsilon()*/;
                left = false;
            } else { // write previous left move and go down
                item.translate({-d+1, 0});
                d = availableSpaceDown(item);
                can_move = d > 1/*std::numeric_limits<Unit>::epsilon()*/;
                left = true;
            }
        }

        if(can_be_packed) {
            Item trsh(item.transformedShape());
            for(auto& v : trsh) can_be_packed = can_be_packed &&
                    getX(v) < bin_.width() &&
                    getY(v) < bin_.height();
        }

        return can_be_packed? PackResult(item) : PackResult();
    }

    void setInitialPosition(Item& item) {
        auto bb = item.boundingBox();

        Vertex v = { getX(bb.maxCorner()), getY(bb.minCorner()) };


        Coord dx = getX(bin_.maxCorner()) - getX(v);
        Coord dy = getY(bin_.maxCorner()) - getY(v);

        item.translate({dx, dy});
    }

    template<class C = Coord>
    static enable_if_t<std::is_floating_point<C>::value, bool>
    isInTheWayOf( const Item& item,
                  const Item& other,
                  const RawShape& scanpoly)
    {
        auto tsh = other.transformedShape();
        return ( ShapeLike::intersects(tsh, scanpoly) ||
                 ShapeLike::isInside(tsh, scanpoly) ) &&
               ( !ShapeLike::intersects(tsh, item.rawShape()) &&
                 !ShapeLike::isInside(tsh, item.rawShape()) );
    }

    template<class C = Coord>
    static enable_if_t<std::is_integral<C>::value, bool>
    isInTheWayOf( const Item& item,
                  const Item& other,
                  const RawShape& scanpoly)
    {
        auto tsh = other.transformedShape();

        bool inters_scanpoly = ShapeLike::intersects(tsh, scanpoly) &&
                !ShapeLike::touches(tsh, scanpoly);
        bool inters_item = ShapeLike::intersects(tsh, item.rawShape()) &&
                !ShapeLike::touches(tsh, item.rawShape());

        return ( inters_scanpoly ||
                 ShapeLike::isInside(tsh, scanpoly)) &&
               ( !inters_item &&
                 !ShapeLike::isInside(tsh, item.rawShape())
                 );
    }

    Container itemsInTheWayOf(const Item& item, const Dir dir) {
        // Get the left or down polygon, that has the same area as the shadow
        // of input item reflected to the left or downwards
        auto&& scanpoly = dir == Dir::LEFT? leftPoly(item) :
                                            downPoly(item);

        Container ret;    // packed items 'in the way' of item
        ret.reserve(items_.size());

        // Predicate to find items that are 'in the way' for left (down) move
        auto predicate = [&scanpoly, &item](const Item& it) {
            return isInTheWayOf(item, it, scanpoly);
        };

        // Get the items that are in the way for the left (or down) movement
        std::copy_if(items_.begin(), items_.end(),
                     std::back_inserter(ret), predicate);

        return ret;
    }

    Unit availableSpace(const Item& _item, const Dir dir) {

        Item item (_item.transformedShape());


        std::function<Coord(const Vertex&)> getCoord;
        std::function< std::pair<Coord, bool>(const Segment&, const Vertex&) >
            availableDistanceSV;

        std::function< std::pair<Coord, bool>(const Vertex&, const Segment&) >
            availableDistance;

        if(dir == Dir::LEFT) {
            getCoord = [](const Vertex& v) { return getX(v); };
            availableDistance = PointLike::horizontalDistance<Vertex>;
            availableDistanceSV = [](const Segment& s, const Vertex& v) {
                auto ret = PointLike::horizontalDistance<Vertex>(v, s);
                if(ret.second) ret.first = -ret.first;
                return ret;
            };
        }
        else {
            getCoord = [](const Vertex& v) { return getY(v); };
            availableDistance = PointLike::verticalDistance<Vertex>;
            availableDistanceSV = [](const Segment& s, const Vertex& v) {
                auto ret = PointLike::verticalDistance<Vertex>(v, s);
                if(ret.second) ret.first = -ret.first;
                return ret;
            };
        }

        auto&& items_in_the_way = itemsInTheWayOf(item, dir);

        // Comparison function for finding min vertex
        auto cmp = [&getCoord](const Vertex& v1, const Vertex& v2) {
            return getCoord(v1) < getCoord(v2);
        };

        // find minimum left or down coordinate of item
        auto minvertex_it = std::min_element(item.begin(),
                                                   item.end(),
                                                   cmp);

        // Get the initial distance in floating point
        Unit m = getCoord(*minvertex_it);

        // Check available distance for every vertex of item to the objects
        // in the way for the nearest intersection
        if(!items_in_the_way.empty()) { // This is crazy, should be optimized...
            for(Item& pleft : items_in_the_way) {
                // For all segments in items_to_left

                assert(pleft.vertexCount() > 0);

                auto trpleft = pleft.transformedShape();
                auto first = ShapeLike::begin(trpleft);
                auto next = first + 1;
                auto endit = ShapeLike::end(trpleft);

                while(next != endit) {
                    Segment seg(*(first++), *(next++));
                    for(auto& v : item) {   // For all vertices in item

                        auto d = availableDistance(v, seg);

                        if(d.second && d.first < m)  m = d.first;
                    }
                }
            }
        }

        if(!items_in_the_way.empty()) {
            auto first = item.begin();
            auto next = first + 1;
            auto endit = item.end();

            // For all edges in item:
            while(next != endit) {
                Segment seg(*(first++), *(next++));

                // for all shapes in items_to_left
                for(Item& sh : items_in_the_way) {
                    assert(sh.vertexCount() > 0);

                    Item tsh(sh.transformedShape());
                    for(auto& v : tsh) {   // For all vertices in item

                        auto d = availableDistanceSV(seg, v);

                        if(d.second && d.first < m)  m = d.first;
                    }
                }
            }
        }

        return m;
    }

    /**
     * Implementation of the left (and down) polygon as described by
     * [López-Camacho et al. 2013]\
     * (http://www.cs.stir.ac.uk/~goc/papers/EffectiveHueristic2DAOR2013.pdf)
     * see algorithm 8 for details...
     */
    RawShape toWallPoly(const Item& _item, const Dir dir) const {
        // The variable names reflect the case of left polygon calculation.
        //
        // We will iterate through the item's vertices and search for the top
        // and bottom vertices (or right and left if dir==Dir::DOWN).
        // Save the relevant vertices and their indices into `bottom` and
        // `top` vectors. In case of left polygon construction these will
        // contain the top and bottom polygons which have the same vertical
        // coordinates (in case there is more of them).
        //
        // We get the leftmost (or downmost) vertex from the `bottom` and `top`
        // vectors and construct the final polygon.

        Item item (_item.transformedShape());

        auto getCoord = [dir](const Vertex& v) {
            return dir == Dir::LEFT? getY(v) : getX(v);
        };

        Coord max_y = std::numeric_limits<Coord>::min();
        Coord min_y = std::numeric_limits<Coord>::max();

        using El = std::pair<size_t, std::reference_wrapper<const Vertex>>;

        std::function<bool(const El&, const El&)> cmp;

        if(dir == Dir::LEFT)
            cmp = [](const El& e1, const El& e2) {
                return getX(e1.second.get()) < getX(e2.second.get());
            };
        else
            cmp = [](const El& e1, const El& e2) {
                return getY(e1.second.get()) < getY(e2.second.get());
            };

        std::vector< El > top;
        std::vector< El > bottom;

        size_t idx = 0;
        for(auto& v : item) { // Find the bottom and top vertices and save them
            auto vref = std::cref(v);
            auto vy = getCoord(v);

            if( vy > max_y ) {
                max_y = vy;
                top.clear();
                top.emplace_back(idx, vref);
            }
            else if(vy == max_y) { top.emplace_back(idx, vref); }

            if(vy < min_y) {
                min_y = vy;
                bottom.clear();
                bottom.emplace_back(idx, vref);
            }
            else if(vy == min_y) { bottom.emplace_back(idx, vref); }

            idx++;
        }

        // Get the top and bottom leftmost vertices, or the right and left
        // downmost vertices (if dir == Dir::DOWN)
        auto topleft_it = std::min_element(top.begin(), top.end(), cmp);
        auto bottomleft_it =
                std::min_element(bottom.begin(), bottom.end(), cmp);

        auto& topleft_vertex = topleft_it->second.get();
        auto& bottomleft_vertex = bottomleft_it->second.get();

        // Start and finish positions for the vertices that will be part of the
        // new polygon
        auto start = std::min(topleft_it->first, bottomleft_it->first);
        auto finish = std::max(topleft_it->first, bottomleft_it->first);

        // the return shape
        RawShape rsh;

        // reserve for all vertices plus 2 for the left horizontal wall, 2 for
        // the additional vertices for maintaning min object distance
        ShapeLike::reserve(rsh, finish-start+4);

        /*auto addOthers = [&rsh, finish, start, &item](){
            for(size_t i = start+1; i < finish; i++)
                ShapeLike::addVertex(rsh, item.vertex(i));
        };*/

        auto reverseAddOthers = [&rsh, finish, start, &item](){
            for(size_t i = finish-1; i > start; i--)
                ShapeLike::addVertex(rsh, item.vertex(i));
        };

        // Final polygon construction...

        if( Item::orientation() == Orientation::CLOCKWISE ) {
            // Clockwise polygon construction

            ShapeLike::addVertex(rsh, topleft_vertex);

            if(dir == Dir::LEFT) reverseAddOthers();
            else {
                ShapeLike::addVertex(rsh, getX(topleft_vertex), 0);
                ShapeLike::addVertex(rsh, getX(bottomleft_vertex), 0);
            }

            ShapeLike::addVertex(rsh, bottomleft_vertex);

            if(dir == Dir::LEFT) {
                ShapeLike::addVertex(rsh, 0, getY(bottomleft_vertex));
                ShapeLike::addVertex(rsh, 0, getY(topleft_vertex));
            }
            else reverseAddOthers();

        } else { // Counter clockwise polygon construction
            /*ShapeLike::addVertex(rsh, topleft_vertex);

            ShapeLike::addVertex(rsh, 0, getCoord(topleft_vertex));
            ShapeLike::addVertex(rsh, 0, getCoord(bottomleft_vertex));

            ShapeLike::addVertex(rsh, bottomleft_vertex);

            if(dir == Dir::LEFT) addOthers();
            else reverseAddOthers();*/

            static_assert(OrientationType<RawShape>::Value ==
                          Orientation::CLOCKWISE,
                          "Counter clockwise toWallPoly() Unimplemented!");

        }

        // Close the polygon
        ShapeLike::addVertex(rsh, topleft_vertex);

        return rsh;
    }

};

}
}

#endif //BOTTOMLEFT_HPP
