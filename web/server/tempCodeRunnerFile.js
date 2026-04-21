
  proposeShare,
  decideShare
} from "./state.js";

const __filename = fileURLToPath(import.meta.url);
const __dirname = path.dirname(__filename);

const app = express();
app.use(express.json());

app.get("/api/graph", (req, res) => {
  res.json(state.graph);
});